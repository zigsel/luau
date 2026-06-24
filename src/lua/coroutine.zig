//! Coroutine / thread control. Free functions taking `*Lua`, re-exported as
//! methods on `Lua`.

const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const errs = @import("errors.zig");
const CoStatus = errs.CoStatus;

/// Outcome of resuming a thread.
pub const ResumeStatus = enum { ok, yield };

/// Yield `nresults` values from the running coroutine. Return the result of this
/// from a C function: `return lua.yield(n);`.
pub fn yield(lua: *Lua, nresults: i32) i32 {
    return c.lua_yield(lua.toRaw(), nresults);
}

/// Yield for a debug break.
pub fn breakCoroutine(lua: *Lua) i32 {
    return c.lua_break(lua.toRaw());
}

/// Start or resume thread `lua` (with `nargs` args on its stack), optionally
/// attributing the resume to thread `from`. `.yield` means it yielded, `.ok`
/// means it finished.
pub fn resumeThread(lua: *Lua, from: ?*Lua, nargs: i32) errs.Error!ResumeStatus {
    const s = c.lua_resume(lua.toRaw(), if (from) |f| f.toRaw() else null, nargs);
    return switch (s) {
        c.LUA_OK => .ok,
        c.LUA_YIELD => .yield,
        else => err: {
            try errs.checkStrict(s); // a non-OK/YIELD status is an error; propagate it
            break :err .ok; // unreachable: checkStrict errors on every other status
        },
    };
}

/// Resume thread `lua` raising the value on its stack as an error.
pub fn resumeError(lua: *Lua, from: ?*Lua) errs.Error!ResumeStatus {
    const s = c.lua_resumeerror(lua.toRaw(), if (from) |f| f.toRaw() else null);
    return switch (s) {
        c.LUA_OK => .ok,
        c.LUA_YIELD => .yield,
        else => err: {
            try errs.checkStrict(s); // a non-OK/YIELD status is an error; propagate it
            break :err .ok; // unreachable: checkStrict errors on every other status
        },
    };
}

/// The thread's own status.
pub fn status(lua: *Lua) errs.Status {
    return errs.Status.fromInt(c.lua_status(lua.toRaw()));
}

/// Whether the running code may yield.
pub fn isYieldable(lua: *Lua) bool {
    return c.lua_isyieldable(lua.toRaw()) != 0;
}

/// The coroutine status of thread `co` as observed from `lua`.
pub fn coStatus(lua: *Lua, co: *Lua) CoStatus {
    return CoStatus.fromInt(c.lua_costatus(lua.toRaw(), co.toRaw()));
}

/// Arbitrary per-thread userdata pointer.
pub fn getThreadData(lua: *Lua) ?*anyopaque {
    return c.lua_getthreaddata(lua.toRaw());
}
pub fn setThreadData(lua: *Lua, data: ?*anyopaque) void {
    c.lua_setthreaddata(lua.toRaw(), data);
}
