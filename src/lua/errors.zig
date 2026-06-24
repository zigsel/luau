//! Status codes and the idiomatic error set layered over them.

const c = @import("bindings");

/// Thread / call status, mirroring `enum lua_Status` (`lua.h`).
pub const Status = enum(c_int) {
    ok = c.LUA_OK, // 0
    yield = c.LUA_YIELD, // 1
    errrun = c.LUA_ERRRUN, // 2
    errsyntax = c.LUA_ERRSYNTAX, // 3 (legacy, preserved for compatibility)
    errmem = c.LUA_ERRMEM, // 4
    errerr = c.LUA_ERRERR, // 5
    break_ = c.LUA_BREAK, // 6 (yielded for a debug breakpoint)
    _,

    pub fn fromInt(v: c_int) Status {
        return @enumFromInt(v);
    }
    pub fn toInt(s: Status) c_int {
        return @intFromEnum(s);
    }
};

/// Coroutine status, mirroring `enum lua_CoStatus` (`lua.h`).
pub const CoStatus = enum(c_int) {
    running = c.LUA_CORUN, // 0
    suspended = c.LUA_COSUS, // 1
    normal = c.LUA_CONOR, // 2
    finished = c.LUA_COFIN, // 3
    @"error" = c.LUA_COERR, // 4
    _,

    pub fn fromInt(v: c_int) CoStatus {
        return @enumFromInt(v);
    }
};

/// Errors raised by protected operations (`pcall`, `load`, `resume`, …).
pub const Error = error{
    /// A runtime error was raised (LUA_ERRRUN); the error value is on the stack.
    Runtime,
    /// A syntax error (LUA_ERRSYNTAX).
    Syntax,
    /// Out of memory (LUA_ERRMEM), or a binding allocation failed.
    Memory,
    /// Error while running the message handler (LUA_ERRERR).
    MessageHandler,
};

/// Translate a raw status code into the idiomatic error set, treating
/// `ok`/`yield`/`break` as success.
pub fn check(status: c_int) Error!void {
    return switch (status) {
        c.LUA_OK, c.LUA_YIELD, c.LUA_BREAK => {},
        c.LUA_ERRSYNTAX => Error.Syntax,
        c.LUA_ERRMEM => Error.Memory,
        c.LUA_ERRERR => Error.MessageHandler,
        else => Error.Runtime,
    };
}

/// Like `check` but requires `ok` exactly (yield/break become `Runtime`).
pub fn checkStrict(status: c_int) Error!void {
    if (status == c.LUA_OK) return;
    return switch (status) {
        c.LUA_ERRSYNTAX => Error.Syntax,
        c.LUA_ERRMEM => Error.Memory,
        c.LUA_ERRERR => Error.MessageHandler,
        else => Error.Runtime,
    };
}
