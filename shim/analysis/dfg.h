// extern "C" shim for the Luau DATA-FLOW GRAPH (Analysis module).
//
// `luau_dfg_build` parses a source string and runs `DataFlowGraphBuilder::build`
// over the parsed `AstStatBlock`, producing a `DataFlowGraph`. The graph is kept
// alive (together with the arenas it borrows) inside an opaque handle.
//
// LIMITATION: the public surface of `DataFlowGraph` is almost entirely
// AST-pointer keyed (getDef(AstExpr*/AstLocal*), getRefinementKey(AstExpr*),
// getSymbolFromDef(Def*)). Those `Def*`/`DefId` values and AST node pointers are
// internal and not meaningfully expressible across the C boundary. So this shim
// binds the builder entry point and exposes only what is cheaply *observable*
// without leaking internal types:
//   - whether parsing and the build succeeded,
//   - the number of top-level statements in the parsed block,
//   - the number of top-level `local` bindings whose `AstLocal*` resolves to a
//     DefId in the graph (a lower bound on the data-flow facts produced).
// Richer queries would require also binding the AST node identity, which is out
// of scope here.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Parse `src` (length `len`) and build its data-flow graph. Always returns a
// handle; query `luau_dfg_ok` for success. Call luau_dfg_free.
LuauDfg* luau_dfg_build(const char* src, size_t len);

// 1 if the source parsed with no errors and the DFG built without throwing.
int luau_dfg_ok(const LuauDfg* h);
// Number of top-level statements in the parsed block (0 on parse failure).
int luau_dfg_statement_count(const LuauDfg* h);
// Number of top-level `local` bindings that resolved to a DefId in the graph.
int luau_dfg_local_def_count(const LuauDfg* h);

void luau_dfg_free(LuauDfg* h);

LUAU_END_DECLS
