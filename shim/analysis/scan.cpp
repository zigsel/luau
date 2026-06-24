// extern "C" shim for self-contained Analysis scans. See scan.h for the contract.
//
// Per-header survey of the requested set, and why each was bound or skipped:
//
//   RequireTracer.h          BOUND. `traceRequires(FileResolver*, AstStatBlock*,
//                            ModuleName, TypeCheckLimits)` is self-contained: it
//                            only needs a FileResolver to map a require argument
//                            to a module name. We supply a resolver that reads
//                            the string literal argument, yielding the list of
//                            statically-known require targets. No solver state.
//
//   AstUtils.h               PARTIALLY BOUND. `matchTypeGuard(op,left,right)` is
//                            a pure AST pattern match -> bound. The three
//                            `findUniqueTypes` overloads need a populated
//                            `DenseHashMap<const AstExpr*, TypeId>` (astTypes)
//                            from a *checked* module, which this parse-only
//                            handle has no access to -> skipped.
//
//   Polarity.h               BOUND. Pure inline bit helpers over the Polarity
//                            enum; re-exposed as plain int functions.
//
//   OverloadResolver.h       SKIPPED. `OverloadResolver`/`resolveOverload`
//                            require a live Normalizer + Subtyping + Scope +
//                            ConstraintSolver context and emit ConstraintV; only
//                            meaningful inside the constraint solver.
//
//   Refinement.h             SKIPPED. `Refinement`/`RefinementId` are Variant
//                            nodes minted by the constraint generator into a
//                            RefinementArena; there is no outside-the-solver
//                            entry point that constructs one to inspect.
//
//   Predicate.h              SKIPPED. Same shape as Refinement (old-solver
//                            PredicateVec built during inference); no public
//                            constructor reachable from a finished checker.
//
//   TableLiteralInference.h  SKIPPED. `pushTypeInto` takes NotNull<
//                            ConstraintSolver>, NotNull<Unifier2>, a live
//                            Constraint and mutable astTypes maps — pure solver
//                            internals.
//
//   RequireTracer covers the require side; the RequireTraceResult `exprs` map
//   (AstNode* -> ModuleInfo) is not exposed since the AstNode* handles are
//   internal to this translation unit; only the requireList is surfaced.
//
//   TypeFunction.h /         SKIPPED. Reduction needs TypeFunctionContext
//   TypeFunctionRuntime.h    (ConstraintSolver-or-equivalent) and the runtime
//                            needs a live lua_State; calling outside the solver
//                            would assert. (Static name/arity is not carried on
//                            these structs in a way reachable here.)
//
//   RecursionCounter.h       SKIPPED. RAII guards that mutate an int* counter;
//                            no meaningful standalone C entry point, and using
//                            them out of context can throw RecursionLimit.

#include "scan.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/AstUtils.h"
#include "Luau/FileResolver.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/Parser.h"
#include "Luau/Polarity.h"
#include "Luau/RequireTracer.h"
#include "Luau/TypeCheckLimits.h"

#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

using namespace Luau;

namespace {

// malloc'd copy of a std::string, for caller-owned char* returns.
static char* dupStr(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

static LuauPosition toPos(const Position& p) {
    LuauPosition out;
    out.line = p.line;
    out.column = p.column;
    return out;
}

// A FileResolver that resolves a require *argument* by reading the string
// literal passed to `require(...)`. This is all `traceRequires` needs to fill
// `requireList` with the statically-known require targets.
struct LiteralResolver : FileResolver {
    std::optional<SourceCode> readSource(const ModuleName&) override {
        return std::nullopt;
    }

    std::optional<ModuleInfo> resolveModule(const ModuleInfo*, AstExpr* expr, const TypeCheckLimits&) override {
        if (auto str = expr->as<AstExprConstantString>())
            return ModuleInfo{std::string(str->value.data, str->value.size), false};
        return std::nullopt;
    }
};

struct GuardHit {
    bool isTypeof;
    std::string type;
    Position begin;
};

// Collect every binary expression that matchTypeGuard recognises as a guard.
struct GuardCollector : AstVisitor {
    std::vector<GuardHit>& out;
    explicit GuardCollector(std::vector<GuardHit>& out) : out(out) {}

    bool visit(AstExprBinary* bin) override {
        if (std::optional<TypeGuard> g = matchTypeGuard(bin->op, bin->left, bin->right))
            out.push_back(GuardHit{g->isTypeof, g->type, bin->location.begin});
        return true;
    }
};

} // namespace

struct LuauScan {
    Allocator allocator;
    AstNameTable names;
    ParseResult result;
    std::vector<std::string> errors;

    LiteralResolver resolver;
    TypeCheckLimits limits;

    std::vector<std::pair<std::string, Position>> requires_;
    std::vector<GuardHit> guards;

    LuauScan() : allocator(), names(allocator) {}
};

extern "C" LuauScan* luau_scan_parse(const char* src, size_t len) {
    LuauScan* h = new LuauScan();
    try {
        ParseOptions options;
        h->result = Parser::parse(src, len, h->names, h->allocator, options);
        for (const ParseError& e : h->result.errors)
            h->errors.push_back(e.getMessage());
    } catch (const std::exception& e) {
        h->errors.push_back(e.what());
    } catch (...) {
        h->errors.push_back("unknown parse error");
    }
    return h;
}

extern "C" int luau_scan_has_root(const LuauScan* h) {
    return (h && h->result.root) ? 1 : 0;
}

extern "C" int luau_scan_error_count(const LuauScan* h) {
    return h ? static_cast<int>(h->errors.size()) : 0;
}

extern "C" const char* luau_scan_error_message(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->errors.size()))
        return nullptr;
    return h->errors[i].c_str();
}

extern "C" void luau_scan_free(LuauScan* h) {
    delete h;
}

// ---- RequireTracer ---------------------------------------------------------

extern "C" int luau_scan_trace_requires(LuauScan* h) {
    if (!h || !h->result.root)
        return -1;
    try {
        RequireTraceResult traced = traceRequires(&h->resolver, h->result.root, "main", h->limits);
        h->requires_.clear();
        h->requires_.reserve(traced.requireList.size());
        for (const auto& entry : traced.requireList)
            h->requires_.push_back({entry.first, entry.second.begin});
        return static_cast<int>(h->requires_.size());
    } catch (const std::exception&) {
        return -1;
    } catch (...) {
        return -1;
    }
}

extern "C" int luau_scan_require_count(const LuauScan* h) {
    return h ? static_cast<int>(h->requires_.size()) : 0;
}

extern "C" char* luau_scan_require_name(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->requires_.size()))
        return nullptr;
    return dupStr(h->requires_[i].first);
}

extern "C" LuauPosition luau_scan_require_position(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->requires_.size()))
        return LuauPosition{0, 0};
    return toPos(h->requires_[i].second);
}

// ---- AstUtils: matchTypeGuard ----------------------------------------------

extern "C" int luau_scan_type_guards(LuauScan* h) {
    if (!h || !h->result.root)
        return -1;
    try {
        h->guards.clear();
        GuardCollector collector{h->guards};
        h->result.root->visit(&collector);
        return static_cast<int>(h->guards.size());
    } catch (const std::exception&) {
        return -1;
    } catch (...) {
        return -1;
    }
}

extern "C" int luau_scan_type_guard_count(const LuauScan* h) {
    return h ? static_cast<int>(h->guards.size()) : 0;
}

extern "C" int luau_scan_type_guard_is_typeof(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->guards.size()))
        return -1;
    return h->guards[i].isTypeof ? 1 : 0;
}

extern "C" char* luau_scan_type_guard_type(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->guards.size()))
        return nullptr;
    return dupStr(h->guards[i].type);
}

extern "C" LuauPosition luau_scan_type_guard_position(const LuauScan* h, int i) {
    if (!h || i < 0 || i >= static_cast<int>(h->guards.size()))
        return LuauPosition{0, 0};
    return toPos(h->guards[i].begin);
}
