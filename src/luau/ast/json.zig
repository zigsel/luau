//! Idiomatic wrapper over "serialize a parsed AST to JSON" (external tooling /
//! editors), via the C++ Analysis shim.

const std = @import("std");
const c = @import("bindings");

/// Parse the self-contained Luau module `src` and serialize its AST to JSON.
///
/// Returns a freshly-allocated JSON string owned by `allocator` (free with
/// `allocator.free`). Returns `error.ParseFailed` if the source could not be
/// parsed, or `error.OutOfMemory` if copying the result fails.
pub fn toJson(allocator: std.mem.Allocator, src: []const u8) ![]u8 {
    var err: [*c]u8 = null;
    const out = c.luau_ast_to_json(src.ptr, src.len, &err);
    if (out == null) {
        if (err != null) std.c.free(err);
        return error.ParseFailed;
    }
    // On success `out` is a malloc'd C string we must take ownership of.
    defer std.c.free(out);
    return try allocator.dupe(u8, std.mem.span(out));
}
