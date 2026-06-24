//! Idiomatic wrapper over TYPE RELATIONS between inferred Luau types via the
//! C++ Analysis shim.
//!
//! `Relations.check` type-checks a self-contained module and retains its type
//! graph; `isSubtype(a, b)` then answers whether the inferred type of top-level
//! binding `a` is a subtype of binding `b`. The result is tri-state: `true`,
//! `false`, or `null` for unknown/error (missing binding or engine failure).

const std = @import("std");
const c = @import("bindings");

pub const Relations = struct {
    handle: *c.LuauRelations,

    /// Type-check the self-contained Luau module `src` (retaining the full type
    /// graph) and return a `Relations` that owns the result. Call `deinit`.
    pub fn check(src: []const u8) Relations {
        return .{ .handle = c.luau_relations_check(src.ptr, src.len).? };
    }

    pub fn deinit(self: Relations) void {
        c.luau_relations_free(self.handle);
    }

    /// Is `type(a) <: type(b)`? Returns `null` on unknown/error (missing
    /// binding, failed check, or an internal engine error).
    pub fn isSubtype(self: Relations, a: [:0]const u8, b: [:0]const u8) ?bool {
        const r = c.luau_relations_is_subtype(self.handle, a.ptr, b.ptr);
        return switch (r) {
            1 => true,
            0 => false,
            else => null,
        };
    }
};
