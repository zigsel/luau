//! The auxiliary library `luaL_*` argument checking / helper family.
//! `check*` functions raise a Luau error (longjmp) on mismatch.

const std = @import("std");
const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const LuaType = @import("enums.zig").LuaType;
const Vector = lua_mod.Vector;

// ---- argument checks (raise on failure) -------------------------------------

pub fn checkString(lua: *Lua, narg: i32) []const u8 {
    var len: usize = 0;
    const p = c.luaL_checklstring(lua.toRaw(), narg, &len);
    return p[0..len];
}
pub fn optString(lua: *Lua, narg: i32, def: [:0]const u8) []const u8 {
    var len: usize = 0;
    const p = c.luaL_optlstring(lua.toRaw(), narg, def.ptr, &len);
    return p[0..len];
}
pub fn checkNumber(lua: *Lua, narg: i32) f64 {
    return c.luaL_checknumber(lua.toRaw(), narg);
}
pub fn optNumber(lua: *Lua, narg: i32, def: f64) f64 {
    return c.luaL_optnumber(lua.toRaw(), narg, def);
}
pub fn checkInteger(lua: *Lua, narg: i32) i32 {
    return c.luaL_checkinteger(lua.toRaw(), narg);
}
pub fn optInteger(lua: *Lua, narg: i32, def: i32) i32 {
    return c.luaL_optinteger(lua.toRaw(), narg, def);
}
pub fn checkInteger64(lua: *Lua, narg: i32) i64 {
    return c.luaL_checkinteger64(lua.toRaw(), narg);
}
pub fn optInteger64(lua: *Lua, narg: i32, def: i64) i64 {
    return c.luaL_optinteger64(lua.toRaw(), narg, def);
}
pub fn checkUnsigned(lua: *Lua, narg: i32) u32 {
    return c.luaL_checkunsigned(lua.toRaw(), narg);
}
pub fn optUnsigned(lua: *Lua, narg: i32, def: u32) u32 {
    return c.luaL_optunsigned(lua.toRaw(), narg, def);
}
pub fn checkBoolean(lua: *Lua, narg: i32) bool {
    return c.luaL_checkboolean(lua.toRaw(), narg) != 0;
}
pub fn optBoolean(lua: *Lua, narg: i32, def: bool) bool {
    return c.luaL_optboolean(lua.toRaw(), narg, @intFromBool(def)) != 0;
}
pub fn checkVector(lua: *Lua, narg: i32) Vector {
    const p = c.luaL_checkvector(lua.toRaw(), narg);
    var v: Vector = undefined;
    inline for (0..lua_mod.vector_size) |i| v[i] = p[i];
    return v;
}
pub fn optVector(lua: *Lua, narg: i32, def: Vector) Vector {
    var d = def;
    const p = c.luaL_optvector(lua.toRaw(), narg, &d[0]);
    var v: Vector = undefined;
    inline for (0..lua_mod.vector_size) |i| v[i] = p[i];
    return v;
}
pub fn checkType(lua: *Lua, narg: i32, t: LuaType) void {
    c.luaL_checktype(lua.toRaw(), narg, t.toInt());
}
pub fn checkAny(lua: *Lua, narg: i32) void {
    c.luaL_checkany(lua.toRaw(), narg);
}
pub fn checkStackMsg(lua: *Lua, sz: i32, msg: [:0]const u8) void {
    c.luaL_checkstack(lua.toRaw(), sz, msg.ptr);
}
pub fn checkUserdata(lua: *Lua, narg: i32, tname: [:0]const u8) *anyopaque {
    return c.luaL_checkudata(lua.toRaw(), narg, tname.ptr).?;
}
pub fn checkBuffer(lua: *Lua, narg: i32) []u8 {
    var len: usize = 0;
    const p = c.luaL_checkbuffer(lua.toRaw(), narg, &len);
    return @as([*]u8, @ptrCast(p))[0..len];
}
pub fn checkOption(lua: *Lua, narg: i32, def: ?[:0]const u8, list: [*:null]const ?[*:0]const u8) i32 {
    return c.luaL_checkoption(lua.toRaw(), narg, if (def) |d| d.ptr else null, @ptrCast(list));
}

// ---- metatables / metafields ------------------------------------------------

/// Create (and register) a new metatable named `tname`, pushing it. Returns
/// false if one already existed.
pub fn newMetatable(lua: *Lua, tname: [:0]const u8) bool {
    return c.luaL_newmetatable(lua.toRaw(), tname.ptr) != 0;
}
/// Push the registered metatable named `tname`.
pub fn getMetatableNamed(lua: *Lua, tname: [:0]const u8) LuaType {
    return LuaType.fromInt(c.lua_getfield(lua.toRaw(), c.LUA_REGISTRYINDEX, tname.ptr));
}
/// Push `obj`'s metafield `e`; returns false if absent.
pub fn getMetafield(lua: *Lua, obj: i32, e: [:0]const u8) bool {
    return c.luaL_getmetafield(lua.toRaw(), obj, e.ptr) != 0;
}
/// Call `obj`'s metamethod `e`; returns false if absent.
pub fn callMeta(lua: *Lua, obj: i32, e: [:0]const u8) bool {
    return c.luaL_callmeta(lua.toRaw(), obj, e.ptr) != 0;
}

// ---- misc auxiliary ---------------------------------------------------------

/// Push `where` info (source:line) for level `lvl`.
pub fn where(lua: *Lua, lvl: i32) void {
    c.luaL_where(lua.toRaw(), lvl);
}
/// Convert the value at `idx` to a string (honouring `__tostring`), pushing it
/// and returning the bytes.
pub fn toStringMeta(lua: *Lua, idx: i32) []const u8 {
    var len: usize = 0;
    const p = c.luaL_tolstring(lua.toRaw(), idx, &len);
    return p[0..len];
}
/// Find or create the nested table `fname` (dotted) under the table at `idx`.
pub fn findTable(lua: *Lua, idx: i32, fname: [:0]const u8, szhint: i32) ?[:0]const u8 {
    return if (c.luaL_findtable(lua.toRaw(), idx, fname.ptr, szhint)) |p| std.mem.span(p) else null;
}
/// Yieldable variant of `call`.
pub fn callYieldable(lua: *Lua, nargs: i32, nresults: i32) i32 {
    return c.luaL_callyieldable(lua.toRaw(), nargs, nresults);
}
/// Yieldable variant of `pcall`.
pub fn pcallYieldable(lua: *Lua, nargs: i32, nresults: i32, errfunc: i32) i32 {
    return c.luaL_pcallyieldable(lua.toRaw(), nargs, nresults, errfunc);
}

// ---- library registration ---------------------------------------------------

/// One library entry (name + function).
pub const Reg = struct { name: [:0]const u8, func: Lua.CFn };

/// Register the functions `regs` into the table on top (or a global library
/// named `libname` if given). Mirrors `luaL_register`.
pub fn register(lua: *Lua, libname: ?[:0]const u8, regs: []const Reg) void {
    var buf: [256]c.luaL_Reg = undefined;
    std.debug.assert(regs.len + 1 <= buf.len);
    for (regs, 0..) |r, i| buf[i] = .{ .name = r.name.ptr, .func = r.func };
    buf[regs.len] = .{ .name = null, .func = null }; // sentinel
    c.luaL_register(lua.toRaw(), if (libname) |l| l.ptr else null, &buf);
}
