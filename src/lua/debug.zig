//! The Luau debug API (`lua.h` debug section).

const std = @import("std");
const c = @import("bindings");
const Lua = @import("lua.zig").Lua;

/// An activation record, mirroring `struct lua_Debug`. Fill it with `getInfo`.
pub const Debug = struct {
    ar: c.lua_Debug = std.mem.zeroes(c.lua_Debug),

    pub fn name(self: *const Debug) ?[:0]const u8 {
        return if (self.ar.name) |p| std.mem.span(p) else null;
    }
    pub fn what(self: *const Debug) ?[:0]const u8 {
        return if (self.ar.what) |p| std.mem.span(p) else null;
    }
    pub fn source(self: *const Debug) ?[:0]const u8 {
        return if (self.ar.source) |p| std.mem.span(p) else null;
    }
    pub fn shortSrc(self: *const Debug) [:0]const u8 {
        return std.mem.span(@as([*:0]const u8, @ptrCast(&self.ar.short_src)));
    }
    pub fn lineDefined(self: *const Debug) i32 {
        return self.ar.linedefined;
    }
    pub fn currentLine(self: *const Debug) i32 {
        return self.ar.currentline;
    }
    pub fn numUpvalues(self: *const Debug) u8 {
        return self.ar.nupvals;
    }
    pub fn numParams(self: *const Debug) u8 {
        return self.ar.nparams;
    }
    pub fn isVararg(self: *const Debug) bool {
        return self.ar.isvararg != 0;
    }
};

/// Number of stack frames.
pub fn stackDepth(lua: *Lua) i32 {
    return c.lua_stackdepth(lua.toRaw());
}

/// Fill `ar` with info about the function at activation `level`. `what` selects
/// fields ("n" name, "s" source, "l" line, "u" upvalues, "a" args, "f" pushes
/// the function). Returns false if the level is invalid.
pub fn getInfo(lua: *Lua, level: i32, what: [:0]const u8, ar: *Debug) bool {
    return c.lua_getinfo(lua.toRaw(), level, what.ptr, &ar.ar) != 0;
}

/// Push the `n`-th argument of the function at `level`; false if unavailable.
pub fn getArgument(lua: *Lua, level: i32, n: i32) bool {
    return c.lua_getargument(lua.toRaw(), level, n) != 0;
}

/// Push the `n`-th local of the function at `level`, returning its name.
pub fn getLocal(lua: *Lua, level: i32, n: i32) ?[:0]const u8 {
    return if (c.lua_getlocal(lua.toRaw(), level, n)) |p| std.mem.span(p) else null;
}
/// Set the `n`-th local from the value on top, returning its name.
pub fn setLocal(lua: *Lua, level: i32, n: i32) ?[:0]const u8 {
    return if (c.lua_setlocal(lua.toRaw(), level, n)) |p| std.mem.span(p) else null;
}
/// Push the `n`-th upvalue of the function at `funcindex`, returning its name.
pub fn getUpvalue(lua: *Lua, funcindex: i32, n: i32) ?[:0]const u8 {
    return if (c.lua_getupvalue(lua.toRaw(), funcindex, n)) |p| std.mem.span(p) else null;
}
/// Set the `n`-th upvalue from the value on top, returning its name.
pub fn setUpvalue(lua: *Lua, funcindex: i32, n: i32) ?[:0]const u8 {
    return if (c.lua_setupvalue(lua.toRaw(), funcindex, n)) |p| std.mem.span(p) else null;
}

/// Enable/disable single-step debugging.
pub fn singleStep(lua: *Lua, enabled: bool) void {
    c.lua_singlestep(lua.toRaw(), @intFromBool(enabled));
}
/// Toggle a breakpoint at `line` in the function at `funcindex`; returns the
/// actual line a breakpoint was set on.
pub fn breakpoint(lua: *Lua, funcindex: i32, line: i32, enabled: bool) i32 {
    return c.lua_breakpoint(lua.toRaw(), funcindex, line, @intFromBool(enabled));
}

/// Raw coverage visitor passthrough.
pub fn getCoverage(lua: *Lua, funcindex: i32, context: ?*anyopaque, callback: c.lua_Coverage) void {
    c.lua_getcoverage(lua.toRaw(), funcindex, context, callback);
}
/// Raw counters visitor passthrough.
pub fn getCounters(lua: *Lua, funcindex: i32, context: ?*anyopaque, fnvisit: c.lua_CounterFunction, ctrvisit: c.lua_CounterValue) void {
    c.lua_getcounters(lua.toRaw(), funcindex, context, fnvisit, ctrvisit);
}

/// A textual backtrace (not thread-safe; debugging only).
pub fn debugTrace(lua: *Lua) [:0]const u8 {
    return std.mem.span(c.lua_debugtrace(lua.toRaw()));
}

/// Push a traceback of `lua1` onto `lua` (optionally prefixed with `msg`).
pub fn traceback(lua: *Lua, lua1: *Lua, msg: ?[:0]const u8, level: i32) void {
    c.luaL_traceback(lua.toRaw(), lua1.toRaw(), if (msg) |m| m.ptr else null, level);
}

/// The VM callback table (interrupt/panic/useratom/…). Advanced; shared between
/// coroutines.
pub fn callbacks(lua: *Lua) *c.lua_Callbacks {
    return @ptrCast(c.lua_callbacks(lua.toRaw()));
}
