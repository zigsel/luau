//! Idiomatic wrapper over type-checking a module against host type definitions.
//!
//! A host can register Luau type-definition source (declaring the types of its
//! API) so that type-checking sees those declarations as builtin globals.

const std = @import("std");
const c = @import("bindings");

/// A 0-based source position.
pub const Position = struct {
    line: u32,
    column: u32,
};

/// A type error from a check.
pub const Error = struct {
    message: []const u8,
    position: Position,
};

/// The result of checking a module against host-augmented globals. Owns the
/// underlying storage; call `deinit`. Diagnostics borrow its storage.
pub const Result = struct {
    handle: *c.LuauDefCheck,

    pub fn deinit(self: Result) void {
        c.luau_analysis_defcheck_free(self.handle);
    }

    /// Whether the definitions source loaded successfully.
    pub fn defsOk(self: Result) bool {
        return c.luau_analysis_defcheck_defs_ok(self.handle) != 0;
    }
    /// Whether checking produced no errors (definition or module).
    pub fn ok(self: Result) bool {
        return self.errorCount() == 0;
    }
    /// Number of errors (including any definition-load error).
    pub fn errorCount(self: Result) usize {
        return @intCast(c.luau_analysis_defcheck_error_count(self.handle));
    }
    /// The `i`-th error (borrows this result's storage).
    pub fn err(self: Result, i: usize) Error {
        const pos = c.luau_analysis_defcheck_error_position(self.handle, @intCast(i));
        return .{
            .message = std.mem.span(c.luau_analysis_defcheck_error_message(self.handle, @intCast(i))),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }
    /// Iterate over all errors.
    pub fn errors(self: Result) ErrorIterator {
        return .{ .result = self, .i = 0, .n = self.errorCount() };
    }
};

pub const ErrorIterator = struct {
    result: Result,
    i: usize,
    n: usize,

    pub fn next(self: *ErrorIterator) ?Error {
        if (self.i >= self.n) return null;
        defer self.i += 1;
        return self.result.err(self.i);
    }
};

/// Type-check `src` against builtin globals augmented with the host type
/// definitions in `defs`.
pub fn checkWithDefinitions(defs: []const u8, src: []const u8) Result {
    return .{ .handle = c.luau_analysis_check_with_defs(defs.ptr, defs.len, src.ptr, src.len).? };
}
