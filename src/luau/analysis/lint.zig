//! Idiomatic wrapper over the Luau LINTER via the C++ Analysis shim.
//!
//! Two capabilities:
//!   - `rules()` enumerates every lint rule (name + numeric code) — the rule set
//!     is compiled into Luau, so this needs no source.
//!   - `lint(src, .{...})` lints a source string with full rule control: an
//!     `enabled` mask selects which rules fire and a `fatal` mask selects which
//!     fired warnings are classified as errors. The result owns its storage.

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

/// One lint rule in the compiled-in rule set.
pub const Rule = struct {
    name: []const u8,
    code: i32,
};

/// Number of lint rules built into Luau.
pub fn ruleCount() usize {
    return @intCast(c.luau_lint_rule_count());
}

/// The `i`-th lint rule (name borrows static storage).
pub fn rule(i: usize) Rule {
    return .{
        .name = std.mem.span(c.luau_lint_rule_name(@intCast(i))),
        .code = c.luau_lint_rule_code(@intCast(i)),
    };
}

/// A bit mask over lint rule codes (bit `code` => that rule). Build one with
/// `Mask.none()`/`Mask.all()` then `.with(code)`, or pass a raw value.
pub const Mask = struct {
    bits: u64,

    pub fn none() Mask {
        return .{ .bits = 0 };
    }
    /// Enable every rule in the compiled-in set.
    pub fn all() Mask {
        var bits: u64 = 0;
        var i: usize = 0;
        while (i < ruleCount()) : (i += 1) bits |= @as(u64, 1) << @intCast(rule(i).code);
        return .{ .bits = bits };
    }
    pub fn with(self: Mask, code: i32) Mask {
        return .{ .bits = self.bits | (@as(u64, 1) << @intCast(code)) };
    }
    pub fn without(self: Mask, code: i32) Mask {
        return .{ .bits = self.bits & ~(@as(u64, 1) << @intCast(code)) };
    }
};

/// Options controlling a standalone lint run.
pub const Options = struct {
    /// Rules that may fire. Defaults to all rules enabled.
    enabled: Mask = Mask{ .bits = std.math.maxInt(u64) },
    /// Rules whose warnings are reported as fatal errors. Defaults to none.
    fatal: Mask = Mask{ .bits = 0 },
};

/// A single lint warning produced by a lint run (borrows the result's storage).
pub const Warning = struct {
    code: i32,
    name: []const u8,
    message: []const u8,
    position: Position,
    /// Whether this warning was classified as fatal (an error).
    fatal: bool,
};

/// The result of a standalone lint run. Owns its storage; call `deinit`.
pub const Result = struct {
    handle: *c.LuauLint,

    pub fn deinit(self: Result) void {
        c.luau_lint_free(self.handle);
    }

    pub fn count(self: Result) usize {
        return @intCast(c.luau_lint_count(self.handle));
    }

    /// The `i`-th warning (fields borrow the result's storage).
    pub fn at(self: Result, i: usize) Warning {
        const p = c.luau_lint_warning_position(self.handle, @intCast(i));
        return .{
            .code = c.luau_lint_warning_code(self.handle, @intCast(i)),
            .name = std.mem.span(c.luau_lint_warning_name(self.handle, @intCast(i))),
            .message = std.mem.span(c.luau_lint_warning_message(self.handle, @intCast(i))),
            .position = .{ .line = p.line, .column = p.column },
            .fatal = c.luau_lint_warning_fatal(self.handle, @intCast(i)) != 0,
        };
    }
};

/// Lint `src` with the given rule control. Caller owns the returned `Result`.
pub fn lint(src: []const u8, options: Options) Result {
    return .{ .handle = c.luau_lint_source(
        src.ptr,
        src.len,
        options.enabled.bits,
        options.fatal.bits,
    ).? };
}
