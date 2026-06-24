//! The remaining `lua_*` surface: userdata tags/dtors/metatables, direct field
//! access, string atoms, tagged-pointer table access, cloning, and assorted
//! helpers.

const std = @import("std");
const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const LuaType = @import("enums.zig").LuaType;

// callback / function-pointer type aliases (so no raw `c` leaks into call sites)
pub const Destructor = c.lua_Destructor;
pub const UserdataDtorFn = ?*const fn (?*anyopaque) callconv(.c) void;
pub const UserdataDirectAccess = c.lua_UserdataDirectAccess;
pub const UserdataDirectNamecall = c.lua_UserdataDirectNamecall;
pub const UserdataDirectFieldGet = c.lua_UserdataDirectFieldGet;

// ---- type query the macro/function pair --------------------------------------

/// Whether the value at `idx` is a 64-bit integer.
pub fn isInteger64(lua: *Lua, idx: i32) bool {
    return c.lua_isinteger64(lua.toRaw(), idx) != 0;
}

// ---- string atoms ------------------------------------------------------------

/// The string at `idx` plus its interned atom id (if a `useratom` callback is
/// installed), or null if not a string.
pub fn toStringAtom(lua: *Lua, idx: i32, atom: *i32) ?[]const u8 {
    var len: usize = 0;
    const p = c.lua_tolstringatom(lua.toRaw(), idx, &len, atom);
    return if (p) |s| s[0..len] else null;
}
/// The NUL-terminated string at `idx` plus its interned atom id, or null.
pub fn toStringAtomZ(lua: *Lua, idx: i32, atom: *i32) ?[:0]const u8 {
    return if (c.lua_tostringatom(lua.toRaw(), idx, atom)) |p| std.mem.span(p) else null;
}
/// The method name being `__namecall`-ed plus its atom, or null.
pub fn namecallAtom(lua: *Lua, atom: *i32) ?[:0]const u8 {
    return if (c.lua_namecallatom(lua.toRaw(), atom)) |p| std.mem.span(p) else null;
}

// ---- tagged light/full userdata ---------------------------------------------

pub fn toLightUserdataTagged(lua: *Lua, idx: i32, tag: i32) ?*anyopaque {
    return c.lua_tolightuserdatatagged(lua.toRaw(), idx, tag);
}
pub fn toUserdataTagged(lua: *Lua, idx: i32, tag: i32) ?*anyopaque {
    return c.lua_touserdatatagged(lua.toRaw(), idx, tag);
}
pub fn userdataTag(lua: *Lua, idx: i32) i32 {
    return c.lua_userdatatag(lua.toRaw(), idx);
}
pub fn lightUserdataTag(lua: *Lua, idx: i32) i32 {
    return c.lua_lightuserdatatag(lua.toRaw(), idx);
}

// ---- userdata creation -------------------------------------------------------

/// Allocate raw tagged userdata of `sz` bytes, pushing it.
pub fn newUserdataTagged(lua: *Lua, sz: usize, tag: i32) ?*anyopaque {
    return c.lua_newuserdatatagged(lua.toRaw(), sz, tag);
}
/// Allocate typed tagged userdata for a `T`, pushing it.
pub fn newUserdata(lua: *Lua, comptime T: type, tag: i32) *T {
    return @ptrCast(@alignCast(c.lua_newuserdatatagged(lua.toRaw(), @sizeOf(T), tag).?));
}
/// Allocate tagged userdata, attaching the metatable registered for `tag`.
pub fn newUserdataWithMetatable(lua: *Lua, sz: usize, tag: i32) ?*anyopaque {
    return c.lua_newuserdatataggedwithmetatable(lua.toRaw(), sz, tag);
}
/// Allocate userdata with a destructor, pushing it.
pub fn newUserdataDtor(lua: *Lua, sz: usize, dtor: UserdataDtorFn) ?*anyopaque {
    return c.lua_newuserdatadtor(lua.toRaw(), sz, dtor);
}
/// Allocate a `sz`-byte buffer object, pushing it; returns the byte slice.
pub fn newBuffer(lua: *Lua, sz: usize) []u8 {
    const p = c.lua_newbuffer(lua.toRaw(), sz).?;
    return @as([*]u8, @ptrCast(p))[0..sz];
}

// ---- userdata tags / dtors / metatables -------------------------------------

pub fn setUserdataTag(lua: *Lua, idx: i32, tag: i32) void {
    c.lua_setuserdatatag(lua.toRaw(), idx, tag);
}
pub fn setUserdataDtor(lua: *Lua, tag: i32, dtor: Destructor) void {
    c.lua_setuserdatadtor(lua.toRaw(), tag, dtor);
}
pub fn getUserdataDtor(lua: *Lua, tag: i32) Destructor {
    return c.lua_getuserdatadtor(lua.toRaw(), tag);
}
pub fn setUserdataMetatable(lua: *Lua, tag: i32) void {
    c.lua_setuserdatametatable(lua.toRaw(), tag);
}
pub fn getUserdataMetatable(lua: *Lua, tag: i32) void {
    c.lua_getuserdatametatable(lua.toRaw(), tag);
}

// ---- direct userdata access (experimental Luau API) -------------------------

pub fn registerUserdataDirectAccess(
    lua: *Lua,
    tag: i32,
    get: UserdataDirectAccess,
    set: UserdataDirectAccess,
    namecall: UserdataDirectNamecall,
) bool {
    return c.lua_registeruserdatadirectaccess(lua.toRaw(), tag, get, set, namecall) != 0;
}
pub fn registerUserdataDirectFieldGet(lua: *Lua, tag: i32, field: [:0]const u8, fn_: UserdataDirectFieldGet) void {
    c.lua_registeruserdatadirectfieldget(lua.toRaw(), tag, field.ptr, fn_);
}
pub fn directFieldSetNumber(result: ?*anyopaque, n: f64) void {
    c.lua_userdatadirectfield_setnumber(result, n);
}
pub fn directFieldSetVector(result: ?*anyopaque, x: f32, y: f32, z: f32) void {
    if (comptime lua_mod.vector_size == 4) {
        c.lua_userdatadirectfield_setvector(result, x, y, z, 0);
    } else {
        c.lua_userdatadirectfield_setvector(result, x, y, z);
    }
}
pub fn directFieldSetBoolean(result: ?*anyopaque, b: bool) void {
    c.lua_userdatadirectfield_setboolean(result, @intFromBool(b));
}
pub fn directFieldSetInteger64(result: ?*anyopaque, n: i64) void {
    c.lua_userdatadirectfield_setinteger64(result, n);
}
pub fn directFieldSetNil(result: ?*anyopaque) void {
    c.lua_userdatadirectfield_setnil(result);
}

// ---- light userdata names ---------------------------------------------------

pub fn setLightUserdataName(lua: *Lua, tag: i32, name: [:0]const u8) void {
    c.lua_setlightuserdataname(lua.toRaw(), tag, name.ptr);
}
pub fn getLightUserdataName(lua: *Lua, tag: i32) ?[:0]const u8 {
    return if (c.lua_getlightuserdataname(lua.toRaw(), tag)) |p| std.mem.span(p) else null;
}

// ---- tagged-pointer table access --------------------------------------------

pub fn rawGetP(lua: *Lua, idx: i32, p: ?*anyopaque) LuaType {
    return LuaType.fromInt(c.lua_rawgetptagged(lua.toRaw(), idx, p, 0));
}
pub fn rawSetP(lua: *Lua, idx: i32, p: ?*anyopaque) void {
    c.lua_rawsetptagged(lua.toRaw(), idx, p, 0);
}
pub fn rawGetPTagged(lua: *Lua, idx: i32, p: ?*anyopaque, tag: i32) LuaType {
    return LuaType.fromInt(c.lua_rawgetptagged(lua.toRaw(), idx, p, tag));
}
pub fn rawSetPTagged(lua: *Lua, idx: i32, p: ?*anyopaque, tag: i32) void {
    c.lua_rawsetptagged(lua.toRaw(), idx, p, tag);
}

// ---- cloning / clearing ------------------------------------------------------

pub fn cloneFunction(lua: *Lua, idx: i32) void {
    c.lua_clonefunction(lua.toRaw(), idx);
}
pub fn clearTable(lua: *Lua, idx: i32) void {
    c.lua_cleartable(lua.toRaw(), idx);
}
pub fn cloneTable(lua: *Lua, idx: i32) void {
    c.lua_clonetable(lua.toRaw(), idx);
}

// ---- assorted ----------------------------------------------------------------

/// Protected call of a C function with a userdata argument.
pub fn cpcall(lua: *Lua, func: Lua.CFn, ud: ?*anyopaque) @import("errors.zig").Error!void {
    try @import("errors.zig").checkStrict(c.lua_cpcall(lua.toRaw(), func, ud));
}
/// Stateless table iteration: returns the next iterator index, or -1 at the end.
pub fn rawIter(lua: *Lua, idx: i32, iter: i32) i32 {
    return c.lua_rawiter(lua.toRaw(), idx, iter);
}
/// Obfuscate a pointer for safe exposure to scripts.
pub fn encodePointer(lua: *Lua, p: usize) usize {
    return c.lua_encodepointer(lua.toRaw(), p);
}
/// A monotonic clock in seconds (process-wide; not tied to a state).
pub fn clock() f64 {
    return c.lua_clock();
}
/// Push a NUL-terminated C string.
pub fn pushStringZ(lua: *Lua, s: [:0]const u8) void {
    c.lua_pushstring(lua.toRaw(), s.ptr);
}
/// Push a preformatted message via Luau's formatter (`%s`).
pub fn pushFString(lua: *Lua, msg: [:0]const u8) void {
    _ = c.lua_pushfstringL(lua.toRaw(), "%s", msg.ptr);
}
