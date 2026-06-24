//! Idiomatic wrapper over the Luau DATA-FLOW GRAPH via the C++ Analysis shim.
//!
//! `Dfg.build(src)` parses a module and runs `DataFlowGraphBuilder::build` over
//! it. The data-flow graph's public surface is almost entirely keyed by internal
//! AST/Def pointers, which are not expressible across the C boundary, so this
//! wrapper exposes only cheaply observable facts: whether the build succeeded,
//! the top-level statement count, and how many top-level `local` bindings
//! resolved to a definition in the graph. See `shim/analysis/dfg.h` for the full
//! limitation note.

const std = @import("std");
const c = @import("bindings");

/// A built data-flow graph. Owns its storage; call `deinit`.
pub const Dfg = struct {
    handle: *c.LuauDfg,

    /// Parse `src` and build its data-flow graph. Caller owns the result.
    pub fn build(src: []const u8) Dfg {
        return .{ .handle = c.luau_dfg_build(src.ptr, src.len).? };
    }

    pub fn deinit(self: Dfg) void {
        c.luau_dfg_free(self.handle);
    }

    /// Whether the source parsed cleanly and the graph built without error.
    pub fn ok(self: Dfg) bool {
        return c.luau_dfg_ok(self.handle) != 0;
    }

    /// Number of top-level statements in the parsed block.
    pub fn statementCount(self: Dfg) usize {
        return @intCast(c.luau_dfg_statement_count(self.handle));
    }

    /// Number of top-level `local` bindings that resolved to a definition.
    pub fn localDefCount(self: Dfg) usize {
        return @intCast(c.luau_dfg_local_def_count(self.handle));
    }
};
