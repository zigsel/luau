//! Idiomatic wrapper over the Luau Ast pretty-printer (via the C++ shim).

const std = @import("std");
const c = @import("bindings");

pub const Error = error{ ParseFailed, OutOfMemory };

/// The parse error captured by the last failed `format` call, if any. Returned
/// as an allocator-owned slice via `formatErr`; for the simpler `format` API a
/// failure just yields `error.ParseFailed`.
pub const FormatError = struct {
    message: []u8,

    pub fn deinit(self: FormatError, allocator: std.mem.Allocator) void {
        allocator.free(self.message);
    }
};

/// Parse `src` and return its pretty-printed (normalized) form. The result is
/// owned by `allocator`. Returns `error.ParseFailed` on a syntax error.
pub fn format(allocator: std.mem.Allocator, src: []const u8) Error![]u8 {
    var err: [*c]u8 = null;
    const out = c.luau_ast_format(src.ptr, src.len, &err);
    if (out == null) {
        if (err) |p| std.c.free(p);
        return error.ParseFailed;
    }
    return dupAndFree(allocator, out) catch error.OutOfMemory;
}

/// Like `format`, but on failure returns the parse error message (owned by
/// `allocator`) instead of a bare error code.
pub fn formatErr(allocator: std.mem.Allocator, src: []const u8) Error!union(enum) {
    ok: []u8,
    fail: FormatError,
} {
    var err: [*c]u8 = null;
    const out = c.luau_ast_format(src.ptr, src.len, &err);
    if (out == null) {
        const msg: []u8 = if (err) |p|
            dupAndFree(allocator, p) catch return error.OutOfMemory
        else
            try allocator.dupe(u8, "parse failed");
        return .{ .fail = .{ .message = msg } };
    }
    return .{ .ok = dupAndFree(allocator, out) catch return error.OutOfMemory };
}

/// Copy a malloc'd C string into an allocator-owned slice, freeing the original.
fn dupAndFree(allocator: std.mem.Allocator, cstr: [*c]u8) ![]u8 {
    defer std.c.free(cstr);
    const span = std.mem.span(@as([*:0]u8, @ptrCast(cstr)));
    return allocator.dupe(u8, span);
}
