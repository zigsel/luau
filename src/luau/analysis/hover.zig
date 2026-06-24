//! Idiomatic wrapper over "what is the inferred type at this position?"
//! (hover / LSP core), via the C++ Analysis shim.

const std = @import("std");
const c = @import("bindings");

/// Infer the type at (`line`, `col`) (0-based) within the self-contained Luau
/// module `src`.
///
/// Returns a freshly-allocated string of the inferred type, or `null` if no
/// type could be resolved at that position. The returned slice is owned by
/// `allocator` and must be freed by the caller. Returns `error.OutOfMemory`
/// if copying the result fails.
pub fn typeAt(allocator: std.mem.Allocator, src: []const u8, line: u32, col: u32) !?[]u8 {
    var out: [*c]u8 = null;
    const ok = c.luau_analysis_type_at(src.ptr, src.len, @intCast(line), @intCast(col), &out);
    if (ok == 0) return null;
    // On success `out` is a malloc'd C string we must take ownership of.
    std.debug.assert(out != null);
    defer std.c.free(out);

    const span = std.mem.span(out);
    if (span.len == 0) return null;
    return try allocator.dupe(u8, span);
}
