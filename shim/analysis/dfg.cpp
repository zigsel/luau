// extern "C" shim over Luau::DataFlowGraphBuilder (Analysis module).

#include "dfg.h"

#include "Luau/Ast.h"
#include "Luau/DataFlowGraph.h"
#include "Luau/Def.h"
#include "Luau/Error.h"
#include "Luau/Lexer.h"
#include "Luau/NotNull.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"

#include <memory>
#include <optional>
#include <string>

using namespace Luau;

// The DataFlowGraph borrows the DefArena/RefinementKeyArena via NotNull, and the
// AST it indexes lives in the Allocator. All of these must outlive the graph, so
// the handle owns them together.
struct LuauDfg {
    Allocator allocator;
    std::unique_ptr<AstNameTable> names;
    DefArena defArena;
    RefinementKeyArena keyArena;
    InternalErrorReporter handle;
    std::optional<DataFlowGraph> graph;

    bool ok = false;
    int statementCount = 0;
    int localDefCount = 0;

    LuauDfg() : names(std::make_unique<AstNameTable>(allocator)) {}
};

extern "C" LuauDfg* luau_dfg_build(const char* src, size_t len) {
    LuauDfg* h = new LuauDfg();
    try {
        ParseOptions options;
        ParseResult result = Parser::parse(src, len, *h->names, h->allocator, options);
        if (!result.root || !result.errors.empty())
            return h; // parse failed; ok stays false

        AstStatBlock* block = result.root;
        h->statementCount = static_cast<int>(block->body.size);

        h->graph.emplace(DataFlowGraphBuilder::build(
            block, NotNull{&h->defArena}, NotNull{&h->keyArena}, NotNull{&h->handle}));
        h->ok = true;

        // Observable lower bound: how many top-level `local` bindings resolved
        // to a DefId in the graph.
        for (AstStat* stat : block->body) {
            if (AstStatLocal* local = stat->as<AstStatLocal>()) {
                for (AstLocal* var : local->vars) {
                    try {
                        (void)h->graph->getDef(var);
                        h->localDefCount++;
                    } catch (...) {
                        // not all locals are guaranteed to have a recorded def
                    }
                }
            }
        }
    } catch (...) {
        h->ok = false;
    }
    return h;
}

extern "C" int luau_dfg_ok(const LuauDfg* h) {
    return h->ok ? 1 : 0;
}
extern "C" int luau_dfg_statement_count(const LuauDfg* h) {
    return h->statementCount;
}
extern "C" int luau_dfg_local_def_count(const LuauDfg* h) {
    return h->localDefCount;
}
extern "C" void luau_dfg_free(LuauDfg* h) {
    delete h;
}
