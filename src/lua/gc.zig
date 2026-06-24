//! Garbage collector control and memory statistics.

const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const GcAction = @import("enums.zig").GcAction;

/// Raw `lua_gc`: perform `action`, with `data` interpreted per the action.
pub fn gc(lua: *Lua, action: GcAction, data: i32) i32 {
    return c.lua_gc(lua.toRaw(), @intFromEnum(action), data);
}

/// Run a full collection cycle.
pub fn gcCollect(lua: *Lua) void {
    _ = c.lua_gc(lua.toRaw(), c.LUA_GCCOLLECT, 0);
}
/// Stop incremental collection.
pub fn gcStop(lua: *Lua) void {
    _ = c.lua_gc(lua.toRaw(), c.LUA_GCSTOP, 0);
}
/// Resume incremental collection.
pub fn gcRestart(lua: *Lua) void {
    _ = c.lua_gc(lua.toRaw(), c.LUA_GCRESTART, 0);
}
/// Perform an explicit GC step of `kilobytes`; returns true if a cycle finished.
pub fn gcStep(lua: *Lua, kilobytes: i32) bool {
    return c.lua_gc(lua.toRaw(), c.LUA_GCSTEP, kilobytes) != 0;
}
/// Current heap size in kilobytes.
pub fn gcCount(lua: *Lua) usize {
    return @intCast(c.lua_gc(lua.toRaw(), c.LUA_GCCOUNT, 0));
}
/// Heap-size remainder in bytes (combine with `gcCount`).
pub fn gcCountBytes(lua: *Lua) usize {
    return @intCast(c.lua_gc(lua.toRaw(), c.LUA_GCCOUNTB, 0));
}
/// Whether the collector is currently running.
pub fn gcIsRunning(lua: *Lua) bool {
    return c.lua_gc(lua.toRaw(), c.LUA_GCISRUNNING, 0) != 0;
}
/// Set the GC goal G (percent of live data the heap may grow to).
pub fn gcSetGoal(lua: *Lua, percent: i32) i32 {
    return c.lua_gc(lua.toRaw(), c.LUA_GCSETGOAL, percent);
}
/// Set the GC step multiplier S (percent).
pub fn gcSetStepMul(lua: *Lua, percent: i32) i32 {
    return c.lua_gc(lua.toRaw(), c.LUA_GCSETSTEPMUL, percent);
}
/// Set the GC step size (kilobytes).
pub fn gcSetStepSize(lua: *Lua, kilobytes: i32) i32 {
    return c.lua_gc(lua.toRaw(), c.LUA_GCSETSTEPSIZE, kilobytes);
}

/// Set the memory category that subsequent allocations are attributed to.
pub fn setMemoryCategory(lua: *Lua, category: i32) void {
    c.lua_setmemcat(lua.toRaw(), category);
}
/// Total bytes currently allocated to `category` (0..LUA_MEMORY_CATEGORIES-1).
pub fn totalBytes(lua: *Lua, category: i32) usize {
    return c.lua_totalbytes(lua.toRaw(), category);
}
