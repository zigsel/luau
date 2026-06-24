//! The idiomatic Luau state wrapper.

const std = @import("std");
const c = @import("bindings");
const config = @import("config");
const errs = @import("errors.zig");
const enums = @import("enums.zig");
const value = @import("value.zig");
const function = @import("function.zig");

pub const Error = errs.Error;
pub const Status = errs.Status;
pub const CoStatus = errs.CoStatus;
pub const LuaType = enums.LuaType;
pub const GcAction = enums.GcAction;

/// A C function callable from Luau (raw signature).
pub const CFn = c.lua_CFunction;
/// A continuation function (raw signature).
pub const Continuation = c.lua_Continuation;
/// The Luau number type (`double`).
pub const Number = f64;
/// The Luau integer type (`int`).
pub const Integer = c_int;
/// The Luau unsigned type (`unsigned`).
pub const Unsigned = c_uint;
/// Number of float lanes in the native vector type (3 or 4), matching the build.
pub const vector_size = config.vector_size;
/// The native vector value.
pub const Vector = [vector_size]f32;

const AllocBox = std.mem.Allocator;
const alloc_alignment: std.mem.Alignment = .fromByteUnits(16);
threadlocal var error_buf: [1024]u8 = undefined;

/// `lua_Alloc` trampoline routing Luau allocations through a Zig allocator.
fn allocTrampoline(ud: ?*anyopaque, ptr: ?*anyopaque, osize: usize, nsize: usize) callconv(.c) ?*anyopaque {
    const box: *AllocBox = @ptrCast(@alignCast(ud.?));
    const a = box.*;
    if (nsize == 0) {
        if (ptr) |p| {
            const mem = @as([*]u8, @ptrCast(p))[0..osize];
            a.vtable.free(a.ptr, mem, alloc_alignment, @returnAddress());
        }
        return null;
    }
    if (ptr == null) {
        // fresh allocation; `osize` is an object-kind tag, not a size — ignore
        return @ptrCast(a.vtable.alloc(a.ptr, nsize, alloc_alignment, @returnAddress()));
    }
    const old = @as([*]u8, @ptrCast(ptr.?))[0..osize];
    if (a.vtable.remap(a.ptr, old, alloc_alignment, nsize, @returnAddress())) |new| {
        return @ptrCast(new);
    }
    const new = a.vtable.alloc(a.ptr, nsize, alloc_alignment, @returnAddress()) orelse return null;
    @memcpy(new[0..@min(osize, nsize)], old[0..@min(osize, nsize)]);
    a.vtable.free(a.ptr, old, alloc_alignment, @returnAddress());
    return @ptrCast(new);
}

/// A Luau state / thread. `*Lua` *is* a `lua_State*` — the wrapper is zero-cost.
pub const Lua = opaque {
    // ---- raw <-> wrapper -------------------------------------------------

    /// Reinterpret this wrapper as the raw `lua_State*` it aliases.
    pub inline fn toRaw(lua: *Lua) *c.lua_State {
        return @ptrCast(lua);
    }
    /// Reinterpret a raw `lua_State*` as the wrapper.
    pub inline fn fromRaw(s: *c.lua_State) *Lua {
        return @ptrCast(s);
    }
    inline fn fromRawOpt(s: ?*c.lua_State) ?*Lua {
        return @ptrCast(s);
    }

    // ---- lifecycle -------------------------------------------------------

    /// Create a new state whose every allocation goes through `gpa`. The
    /// allocator is boxed (stable address) and owned by the state until
    /// `deinit`; `gpa` must outlive the state.
    pub fn init(gpa: std.mem.Allocator) error{OutOfMemory}!*Lua {
        const box = try gpa.create(AllocBox);
        box.* = gpa;
        const state = c.lua_newstate(allocTrampoline, box) orelse {
            gpa.destroy(box);
            return error.OutOfMemory;
        };
        return fromRaw(state);
    }

    /// Create a new state using Luau's built-in (libc `malloc`) allocator.
    /// Pair with `close` rather than `deinit`.
    pub fn initLibc() error{OutOfMemory}!*Lua {
        return fromRawOpt(c.luaL_newstate()) orelse error.OutOfMemory;
    }

    /// Create a new state with an explicit raw `lua_Alloc`/userdata pair.
    pub fn newState(alloc_fn: c.lua_Alloc, ud: ?*anyopaque) error{OutOfMemory}!*Lua {
        return fromRawOpt(c.lua_newstate(alloc_fn, ud)) orelse error.OutOfMemory;
    }

    /// Destroy a state created with `init`, also freeing the boxed allocator.
    pub fn deinit(lua: *Lua) void {
        var ud: ?*anyopaque = null;
        _ = c.lua_getallocf(lua.toRaw(), &ud);
        c.lua_close(lua.toRaw());
        if (ud) |u| {
            const box: *AllocBox = @ptrCast(@alignCast(u));
            const gpa = box.*;
            gpa.destroy(box);
        }
    }

    /// Destroy a state (raw `lua_close`). Use with `initLibc`/`newState`.
    pub fn close(lua: *Lua) void {
        c.lua_close(lua.toRaw());
    }

    /// The Zig allocator backing this state (for states created with `init`).
    pub fn allocator(lua: *Lua) std.mem.Allocator {
        var ud: ?*anyopaque = null;
        _ = c.lua_getallocf(lua.toRaw(), &ud);
        const box: *AllocBox = @ptrCast(@alignCast(ud.?));
        return box.*;
    }

    /// Create a new coroutine thread sharing the global environment.
    pub fn newThread(lua: *Lua) *Lua {
        return fromRaw(c.lua_newthread(lua.toRaw()).?);
    }
    /// The main thread of the state.
    pub fn mainThread(lua: *Lua) *Lua {
        return fromRaw(c.lua_mainthread(lua.toRaw()).?);
    }
    /// Reset a thread to a clean reusable state.
    pub fn resetThread(lua: *Lua) void {
        c.lua_resetthread(lua.toRaw());
    }
    /// Whether the thread has been reset and is ready for reuse.
    pub fn isThreadReset(lua: *Lua) bool {
        return c.lua_isthreadreset(lua.toRaw()) != 0;
    }

    // ---- raising errors (longjmp; never returns) -------------------------

    /// Raise a Luau error with a formatted message (unwinds via longjmp).
    pub fn raiseError(lua: *Lua, comptime fmt: []const u8, args: anytype) noreturn {
        const msg = std.fmt.bufPrintZ(&error_buf, fmt, args) catch blk: {
            error_buf[error_buf.len - 1] = 0;
            break :blk error_buf[0 .. error_buf.len - 1 :0];
        };
        c.luaL_errorL(lua.toRaw(), "%s", msg.ptr);
    }
    /// Raise a Luau error with a NUL-terminated message.
    pub fn raiseErrorStr(lua: *Lua, msg: [:0]const u8) noreturn {
        c.luaL_errorL(lua.toRaw(), "%s", msg.ptr);
    }
    /// Raise the value on top of the stack as an error.
    pub fn raise(lua: *Lua) noreturn {
        c.lua_error(lua.toRaw());
    }
    /// Raise an "argument #narg" error with extra context.
    pub fn argError(lua: *Lua, narg: i32, extramsg: [:0]const u8) noreturn {
        c.luaL_argerrorL(lua.toRaw(), narg, extramsg.ptr);
    }
    /// Raise a "wrong type for argument #narg" error.
    pub fn typeError(lua: *Lua, narg: i32, tname: [:0]const u8) noreturn {
        c.luaL_typeerrorL(lua.toRaw(), narg, tname.ptr);
    }

    // ---- basic stack manipulation ----------------------------------------

    pub fn absIndex(lua: *Lua, idx: i32) i32 {
        return c.lua_absindex(lua.toRaw(), idx);
    }
    /// Number of elements on the stack (also the top index).
    pub fn getTop(lua: *Lua) i32 {
        return c.lua_gettop(lua.toRaw());
    }
    pub fn setTop(lua: *Lua, idx: i32) void {
        c.lua_settop(lua.toRaw(), idx);
    }
    pub fn pop(lua: *Lua, n: i32) void {
        c.lua_settop(lua.toRaw(), -n - 1);
    }
    pub fn pushValue(lua: *Lua, idx: i32) void {
        c.lua_pushvalue(lua.toRaw(), idx);
    }
    pub fn remove(lua: *Lua, idx: i32) void {
        c.lua_remove(lua.toRaw(), idx);
    }
    pub fn insert(lua: *Lua, idx: i32) void {
        c.lua_insert(lua.toRaw(), idx);
    }
    pub fn replace(lua: *Lua, idx: i32) void {
        c.lua_replace(lua.toRaw(), idx);
    }
    pub fn checkStack(lua: *Lua, sz: i32) bool {
        return c.lua_checkstack(lua.toRaw(), sz) != 0;
    }
    pub fn rawCheckStack(lua: *Lua, sz: i32) void {
        c.lua_rawcheckstack(lua.toRaw(), sz);
    }
    pub fn xMove(from: *Lua, to: *Lua, n: i32) void {
        c.lua_xmove(from.toRaw(), to.toRaw(), n);
    }
    pub fn xPush(from: *Lua, to: *Lua, idx: i32) void {
        c.lua_xpush(from.toRaw(), to.toRaw(), idx);
    }

    // ---- type queries & access (stack -> Zig) ----------------------------

    pub fn typeOf(lua: *Lua, idx: i32) LuaType {
        return LuaType.fromInt(c.lua_type(lua.toRaw(), idx));
    }
    pub fn typeName(lua: *Lua, t: LuaType) [:0]const u8 {
        return std.mem.span(c.lua_typename(lua.toRaw(), t.toInt()));
    }
    pub fn typeNameAt(lua: *Lua, idx: i32) [:0]const u8 {
        return std.mem.span(c.luaL_typename(lua.toRaw(), idx));
    }

    pub fn isNumber(lua: *Lua, idx: i32) bool {
        return c.lua_isnumber(lua.toRaw(), idx) != 0;
    }
    pub fn isString(lua: *Lua, idx: i32) bool {
        return c.lua_isstring(lua.toRaw(), idx) != 0;
    }
    pub fn isCFunction(lua: *Lua, idx: i32) bool {
        return c.lua_iscfunction(lua.toRaw(), idx) != 0;
    }
    pub fn isLuaFunction(lua: *Lua, idx: i32) bool {
        return c.lua_isLfunction(lua.toRaw(), idx) != 0;
    }
    pub fn isUserdata(lua: *Lua, idx: i32) bool {
        return c.lua_isuserdata(lua.toRaw(), idx) != 0;
    }
    pub fn isNil(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .nil;
    }
    pub fn isBoolean(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .boolean;
    }
    pub fn isTable(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .table;
    }
    pub fn isFunction(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .function;
    }
    pub fn isThread(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .thread;
    }
    pub fn isVector(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .vector;
    }
    pub fn isBuffer(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .buffer;
    }
    pub fn isNone(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .none;
    }
    pub fn isNoneOrNil(lua: *Lua, idx: i32) bool {
        return @intFromEnum(lua.typeOf(idx)) <= c.LUA_TNIL;
    }
    pub fn isLightUserdata(lua: *Lua, idx: i32) bool {
        return lua.typeOf(idx) == .lightuserdata;
    }

    pub fn equal(lua: *Lua, idx1: i32, idx2: i32) bool {
        return c.lua_equal(lua.toRaw(), idx1, idx2) != 0;
    }
    pub fn rawEqual(lua: *Lua, idx1: i32, idx2: i32) bool {
        return c.lua_rawequal(lua.toRaw(), idx1, idx2) != 0;
    }
    pub fn lessThan(lua: *Lua, idx1: i32, idx2: i32) bool {
        return c.lua_lessthan(lua.toRaw(), idx1, idx2) != 0;
    }

    pub fn toNumber(lua: *Lua, idx: i32) ?Number {
        var ok: c_int = 0;
        const n = c.lua_tonumberx(lua.toRaw(), idx, &ok);
        return if (ok != 0) n else null;
    }
    pub fn toInteger(lua: *Lua, idx: i32) ?Integer {
        var ok: c_int = 0;
        const n = c.lua_tointegerx(lua.toRaw(), idx, &ok);
        return if (ok != 0) n else null;
    }
    pub fn toUnsigned(lua: *Lua, idx: i32) ?Unsigned {
        var ok: c_int = 0;
        const n = c.lua_tounsignedx(lua.toRaw(), idx, &ok);
        return if (ok != 0) n else null;
    }
    pub fn toInteger64(lua: *Lua, idx: i32) ?i64 {
        var ok: c_int = 0;
        const n = c.lua_tointeger64(lua.toRaw(), idx, &ok);
        return if (ok != 0) n else null;
    }
    pub fn toBoolean(lua: *Lua, idx: i32) bool {
        return c.lua_toboolean(lua.toRaw(), idx) != 0;
    }
    /// The value at `idx` as a byte slice (valid while it stays on the stack),
    /// or null if not a string/number. Embedded NULs preserved.
    pub fn toString(lua: *Lua, idx: i32) ?[]const u8 {
        var len: usize = 0;
        const ptr = c.lua_tolstring(lua.toRaw(), idx, &len);
        return if (ptr) |p| p[0..len] else null;
    }
    pub fn toVector(lua: *Lua, idx: i32) ?Vector {
        const ptr = c.lua_tovector(lua.toRaw(), idx);
        if (ptr == null) return null;
        var v: Vector = undefined;
        inline for (0..vector_size) |i| v[i] = ptr[i];
        return v;
    }
    pub fn toCFunction(lua: *Lua, idx: i32) CFn {
        return c.lua_tocfunction(lua.toRaw(), idx);
    }
    pub fn toLightUserdata(lua: *Lua, idx: i32) ?*anyopaque {
        return c.lua_tolightuserdata(lua.toRaw(), idx);
    }
    pub fn toUserdata(lua: *Lua, idx: i32) ?*anyopaque {
        return c.lua_touserdata(lua.toRaw(), idx);
    }
    /// The userdata at `idx` typed as `*T` (no tag check), or null.
    pub fn toUserdataPtr(lua: *Lua, comptime T: type, idx: i32) ?*T {
        return @ptrCast(@alignCast(c.lua_touserdata(lua.toRaw(), idx) orelse return null));
    }
    pub fn toThread(lua: *Lua, idx: i32) ?*Lua {
        return fromRawOpt(c.lua_tothread(lua.toRaw(), idx));
    }
    pub fn toBuffer(lua: *Lua, idx: i32) ?[]u8 {
        var len: usize = 0;
        const ptr = c.lua_tobuffer(lua.toRaw(), idx, &len);
        return if (ptr) |p| @as([*]u8, @ptrCast(p))[0..len] else null;
    }
    pub fn toPointer(lua: *Lua, idx: i32) ?*const anyopaque {
        return c.lua_topointer(lua.toRaw(), idx);
    }
    pub fn objLen(lua: *Lua, idx: i32) usize {
        return @intCast(c.lua_objlen(lua.toRaw(), idx));
    }

    // ---- push (Zig -> stack) ---------------------------------------------

    pub fn pushNil(lua: *Lua) void {
        c.lua_pushnil(lua.toRaw());
    }
    pub fn pushNumber(lua: *Lua, n: Number) void {
        c.lua_pushnumber(lua.toRaw(), n);
    }
    pub fn pushInteger(lua: *Lua, n: Integer) void {
        c.lua_pushinteger(lua.toRaw(), n);
    }
    pub fn pushInteger64(lua: *Lua, n: i64) void {
        c.lua_pushinteger64(lua.toRaw(), n);
    }
    pub fn pushUnsigned(lua: *Lua, n: Unsigned) void {
        c.lua_pushunsigned(lua.toRaw(), n);
    }
    pub fn pushBoolean(lua: *Lua, b: bool) void {
        c.lua_pushboolean(lua.toRaw(), @intFromBool(b));
    }
    /// Push a string (copied into the VM); any byte slice, NULs preserved.
    pub fn pushString(lua: *Lua, s: []const u8) void {
        c.lua_pushlstring(lua.toRaw(), s.ptr, s.len);
    }
    /// Push the native vector `(x, y, z)` (`w = 0` when LUA_VECTOR_SIZE == 4).
    pub fn pushVector(lua: *Lua, x: f32, y: f32, z: f32) void {
        if (comptime vector_size == 4) {
            c.lua_pushvector(lua.toRaw(), x, y, z, 0);
        } else {
            c.lua_pushvector(lua.toRaw(), x, y, z);
        }
    }
    /// Push the 4-lane native vector (`w` ignored unless LUA_VECTOR_SIZE == 4).
    pub fn pushVector4(lua: *Lua, x: f32, y: f32, z: f32, w: f32) void {
        if (comptime vector_size == 4) {
            c.lua_pushvector(lua.toRaw(), x, y, z, w);
        } else {
            c.lua_pushvector(lua.toRaw(), x, y, z);
        }
    }
    pub fn pushLightUserdata(lua: *Lua, p: ?*anyopaque) void {
        c.lua_pushlightuserdatatagged(lua.toRaw(), p, 0);
    }
    pub fn pushLightUserdataTagged(lua: *Lua, p: ?*anyopaque, tag: i32) void {
        c.lua_pushlightuserdatatagged(lua.toRaw(), p, tag);
    }
    /// Push the current thread; returns true if it is the main thread.
    pub fn pushThread(lua: *Lua) bool {
        return c.lua_pushthread(lua.toRaw()) != 0;
    }

    // ---- pushing functions (comptime Zig -> C wrapping) ------------------

    /// Push a Zig function as a Luau closure with `nups` captured upvalues.
    pub fn pushClosure(lua: *Lua, comptime func: anytype, name: ?[:0]const u8, nups: i32) void {
        c.lua_pushcclosurek(lua.toRaw(), function.wrap(func), if (name) |n| n.ptr else null, nups, null);
    }
    /// Push a Zig function as a Luau function (no upvalues).
    pub fn pushFunction(lua: *Lua, comptime func: anytype, name: ?[:0]const u8) void {
        lua.pushClosure(func, name, 0);
    }
    /// Push an already-C-ABI `lua_CFunction` directly (no wrapping).
    pub fn pushCFunction(lua: *Lua, func: CFn, name: ?[:0]const u8) void {
        c.lua_pushcclosurek(lua.toRaw(), func, if (name) |n| n.ptr else null, 0, null);
    }
    /// Push an already-C-ABI closure with upvalues and a continuation.
    pub fn pushCClosure(lua: *Lua, func: CFn, name: ?[:0]const u8, nups: i32, cont: Continuation) void {
        c.lua_pushcclosurek(lua.toRaw(), func, if (name) |n| n.ptr else null, nups, cont);
    }
    /// The pseudo-index of the `i`-th upvalue of the running C closure.
    pub fn upvalueIndex(i: i32) i32 {
        return c.LUA_GLOBALSINDEX - i;
    }

    // ---- get (table -> stack) --------------------------------------------

    pub fn getTable(lua: *Lua, idx: i32) LuaType {
        return LuaType.fromInt(c.lua_gettable(lua.toRaw(), idx));
    }
    pub fn getField(lua: *Lua, idx: i32, name: [:0]const u8) LuaType {
        return LuaType.fromInt(c.lua_getfield(lua.toRaw(), idx, name.ptr));
    }
    pub fn rawGetField(lua: *Lua, idx: i32, name: [:0]const u8) LuaType {
        return LuaType.fromInt(c.lua_rawgetfield(lua.toRaw(), idx, name.ptr));
    }
    pub fn rawGet(lua: *Lua, idx: i32) LuaType {
        return LuaType.fromInt(c.lua_rawget(lua.toRaw(), idx));
    }
    pub fn rawGetIndex(lua: *Lua, idx: i32, n: i32) LuaType {
        return LuaType.fromInt(c.lua_rawgeti(lua.toRaw(), idx, n));
    }
    pub fn createTable(lua: *Lua, narr: i32, nrec: i32) void {
        c.lua_createtable(lua.toRaw(), narr, nrec);
    }
    pub fn newTable(lua: *Lua) void {
        c.lua_createtable(lua.toRaw(), 0, 0);
    }
    pub fn getMetatable(lua: *Lua, idx: i32) bool {
        return c.lua_getmetatable(lua.toRaw(), idx) != 0;
    }
    pub fn getFenv(lua: *Lua, idx: i32) void {
        c.lua_getfenv(lua.toRaw(), idx);
    }
    pub fn setReadonly(lua: *Lua, idx: i32, enabled: bool) void {
        c.lua_setreadonly(lua.toRaw(), idx, @intFromBool(enabled));
    }
    pub fn getReadonly(lua: *Lua, idx: i32) bool {
        return c.lua_getreadonly(lua.toRaw(), idx) != 0;
    }
    pub fn setSafeEnv(lua: *Lua, idx: i32, enabled: bool) void {
        c.lua_setsafeenv(lua.toRaw(), idx, @intFromBool(enabled));
    }

    // ---- set (stack -> table) --------------------------------------------

    pub fn setTable(lua: *Lua, idx: i32) void {
        c.lua_settable(lua.toRaw(), idx);
    }
    pub fn setField(lua: *Lua, idx: i32, name: [:0]const u8) void {
        c.lua_setfield(lua.toRaw(), idx, name.ptr);
    }
    pub fn rawSetField(lua: *Lua, idx: i32, name: [:0]const u8) void {
        c.lua_rawsetfield(lua.toRaw(), idx, name.ptr);
    }
    pub fn rawSet(lua: *Lua, idx: i32) void {
        c.lua_rawset(lua.toRaw(), idx);
    }
    pub fn rawSetIndex(lua: *Lua, idx: i32, n: i32) void {
        c.lua_rawseti(lua.toRaw(), idx, n);
    }
    pub fn setMetatable(lua: *Lua, idx: i32) void {
        _ = c.lua_setmetatable(lua.toRaw(), idx);
    }
    pub fn setFenv(lua: *Lua, idx: i32) bool {
        return c.lua_setfenv(lua.toRaw(), idx) != 0;
    }

    // ---- globals ---------------------------------------------------------

    pub fn getGlobal(lua: *Lua, name: [:0]const u8) LuaType {
        return LuaType.fromInt(c.lua_getfield(lua.toRaw(), c.LUA_GLOBALSINDEX, name.ptr));
    }
    pub fn setGlobal(lua: *Lua, name: [:0]const u8) void {
        c.lua_setfield(lua.toRaw(), c.LUA_GLOBALSINDEX, name.ptr);
    }

    // ---- load & call -----------------------------------------------------

    /// Load precompiled `bytecode` as a function, pushing it on success.
    pub fn loadBytecode(lua: *Lua, chunkname: [:0]const u8, bytecode: []const u8, env: i32) Error!void {
        try errs.checkStrict(c.luau_load(lua.toRaw(), chunkname.ptr, bytecode.ptr, bytecode.len, env));
    }
    /// Compile `source` with default options and load it, pushing the function.
    pub fn loadString(lua: *Lua, chunkname: [:0]const u8, source: []const u8) Error!void {
        var len: usize = 0;
        const bc = c.luau_compile(source.ptr, source.len, null, &len);
        defer std.c.free(@ptrCast(bc));
        if (bc == null) return Error.Memory;
        return lua.loadBytecode(chunkname, bc[0..len], 0);
    }
    /// Call function + args on the stack, expecting `nresults` (unprotected).
    pub fn call(lua: *Lua, nargs: i32, nresults: i32) void {
        c.lua_call(lua.toRaw(), nargs, nresults);
    }
    /// Protected call. On error leaves the error value on the stack; `errfunc`
    /// is the stack index of a message handler (0 for none).
    pub fn pcall(lua: *Lua, nargs: i32, nresults: i32, errfunc: i32) Error!void {
        try errs.checkStrict(c.lua_pcall(lua.toRaw(), nargs, nresults, errfunc));
    }
    /// Compile, load, and protected-call `source` for its side effects.
    pub fn doString(lua: *Lua, chunkname: [:0]const u8, source: []const u8) Error!void {
        try lua.loadString(chunkname, source);
        try lua.pcall(0, 0, 0);
    }

    // ---- standard libraries & sandboxing ---------------------------------

    pub fn openLibs(lua: *Lua) void {
        c.luaL_openlibs(lua.toRaw());
    }
    pub fn openBase(lua: *Lua) void {
        _ = c.luaopen_base(lua.toRaw());
    }
    pub fn openCoroutine(lua: *Lua) void {
        _ = c.luaopen_coroutine(lua.toRaw());
    }
    pub fn openTable(lua: *Lua) void {
        _ = c.luaopen_table(lua.toRaw());
    }
    pub fn openOs(lua: *Lua) void {
        _ = c.luaopen_os(lua.toRaw());
    }
    pub fn openString(lua: *Lua) void {
        _ = c.luaopen_string(lua.toRaw());
    }
    pub fn openBit32(lua: *Lua) void {
        _ = c.luaopen_bit32(lua.toRaw());
    }
    pub fn openBuffer(lua: *Lua) void {
        _ = c.luaopen_buffer(lua.toRaw());
    }
    pub fn openUtf8(lua: *Lua) void {
        _ = c.luaopen_utf8(lua.toRaw());
    }
    pub fn openClass(lua: *Lua) void {
        _ = c.luaopen_class(lua.toRaw());
    }
    pub fn openMath(lua: *Lua) void {
        _ = c.luaopen_math(lua.toRaw());
    }
    pub fn openDebug(lua: *Lua) void {
        _ = c.luaopen_debug(lua.toRaw());
    }
    pub fn openVector(lua: *Lua) void {
        _ = c.luaopen_vector(lua.toRaw());
    }
    pub fn openInteger(lua: *Lua) void {
        _ = c.luaopen_integer(lua.toRaw());
    }
    pub fn sandbox(lua: *Lua) void {
        c.luaL_sandbox(lua.toRaw());
    }
    pub fn sandboxThread(lua: *Lua) void {
        c.luaL_sandboxthread(lua.toRaw());
    }

    // ---- references ------------------------------------------------------

    pub fn ref(lua: *Lua, idx: i32) i32 {
        return c.lua_ref(lua.toRaw(), idx);
    }
    pub fn unref(lua: *Lua, r: i32) void {
        c.lua_unref(lua.toRaw(), r);
    }
    pub fn getRef(lua: *Lua, r: i32) LuaType {
        return LuaType.fromInt(c.lua_rawgeti(lua.toRaw(), c.LUA_REGISTRYINDEX, r));
    }

    // ---- misc ------------------------------------------------------------

    pub fn concat(lua: *Lua, n: i32) void {
        c.lua_concat(lua.toRaw(), n);
    }
    /// Table traversal: pops a key, pushes the next pair; false at end.
    pub fn next(lua: *Lua, idx: i32) bool {
        return c.lua_next(lua.toRaw(), idx) != 0;
    }

    // ---- high-level marshalling (sol-style) — see value.zig --------------

    /// Push any supported Zig value as the natural Luau value.
    pub const push = value.push;
    /// Read the value at an index as a `T` (`error.LuaTypeMismatch` on mismatch).
    pub const pull = value.pull;
    /// Set a global to any marshalled Zig value: `lua.set("x", 42)`.
    pub const set = value.set;
    /// Read global `name` as `T`, or a default if missing/wrong type.
    pub const getOr = value.getOr;
    /// The globals table as an ergonomic `Table` handle (for `getPath`/`setPath`).
    pub fn globals(lua: *Lua) @import("table.zig").Table {
        lua.pushValue(c.LUA_GLOBALSINDEX);
        defer lua.pop(1);
        return @import("table.zig").Table.fromStack(lua, -1);
    }
    /// Read a global as a `T`: `const n = try lua.get(i32, "x")`.
    pub const get = value.get;
    /// Register a Zig function as a global (auto-marshalled): `lua.setFn("add", add)`.
    pub const setFn = function.setFn;
    /// Register a *closure*: a Zig function bundled with a captured pointer that
    /// arrives as its first parameter. `lua.setCapture("inc", &counter, inc)`.
    pub const setCapture = function.setCapture;
    /// Register an *overloaded* function dispatched by argument count:
    /// `lua.setOverload("f", .{ zero, one, two })`.
    pub const setOverload = function.setOverload;
    /// Register a Zig struct as a sol-style usertype: `lua.registerType(Vec2)`.
    pub const registerType = @import("usertype.zig").registerType;
    /// Expose a Zig namespace as a Luau library: `lua.bindModule("vec", VecLib)`.
    pub const bindModule = @import("usertype.zig").bindModule;
    /// Allocate a registered usertype instance as userdata on the stack.
    pub const pushInstance = @import("usertype.zig").pushInstance;
    /// Get a pointer to the `T` instance at `idx`, or null if it is not one.
    pub const getInstance = @import("usertype.zig").getInstance;

    // ---- coroutines (see coroutine.zig) ----------------------------------
    const coro = @import("coroutine.zig");
    pub const ResumeStatus = coro.ResumeStatus;
    pub const yield = coro.yield;
    pub const breakCoroutine = coro.breakCoroutine;
    pub const resumeThread = coro.resumeThread;
    pub const resumeError = coro.resumeError;
    pub const status = coro.status;
    pub const isYieldable = coro.isYieldable;
    pub const coStatus = coro.coStatus;
    pub const getThreadData = coro.getThreadData;
    pub const setThreadData = coro.setThreadData;

    // ---- garbage collector (see gc.zig) ----------------------------------
    const gc_mod = @import("gc.zig");
    pub const gc = gc_mod.gc;
    pub const gcCollect = gc_mod.gcCollect;
    pub const gcStop = gc_mod.gcStop;
    pub const gcRestart = gc_mod.gcRestart;
    pub const gcStep = gc_mod.gcStep;
    pub const gcCount = gc_mod.gcCount;
    pub const gcCountBytes = gc_mod.gcCountBytes;
    pub const gcIsRunning = gc_mod.gcIsRunning;
    pub const gcSetGoal = gc_mod.gcSetGoal;
    pub const gcSetStepMul = gc_mod.gcSetStepMul;
    pub const gcSetStepSize = gc_mod.gcSetStepSize;
    pub const setMemoryCategory = gc_mod.setMemoryCategory;
    pub const totalBytes = gc_mod.totalBytes;

    // ---- native codegen (see codegen.zig) --------------------------------
    const codegen_mod = @import("codegen.zig");
    /// Create a JIT code generator for this state (call once, after init).
    pub const codegenCreate = codegen_mod.create;
    /// Natively compile the function at `idx` and its inner functions.
    pub const codegenCompile = codegen_mod.compile;

    // ---- debug API (see debug.zig) ---------------------------------------
    const debug_mod = @import("debug.zig");
    pub const Debug = debug_mod.Debug;
    pub const stackDepth = debug_mod.stackDepth;
    pub const getInfo = debug_mod.getInfo;
    pub const getArgument = debug_mod.getArgument;
    pub const getLocal = debug_mod.getLocal;
    pub const setLocal = debug_mod.setLocal;
    pub const getUpvalue = debug_mod.getUpvalue;
    pub const setUpvalue = debug_mod.setUpvalue;
    pub const singleStep = debug_mod.singleStep;
    pub const breakpoint = debug_mod.breakpoint;
    pub const getCoverage = debug_mod.getCoverage;
    pub const getCounters = debug_mod.getCounters;
    pub const debugTrace = debug_mod.debugTrace;
    pub const traceback = debug_mod.traceback;
    pub const callbacks = debug_mod.callbacks;

    // ---- auxiliary library (see auxlib.zig) ------------------------------
    const aux = @import("auxlib.zig");
    pub const Reg = aux.Reg;
    pub const checkString = aux.checkString;
    pub const optString = aux.optString;
    pub const checkNumber = aux.checkNumber;
    pub const optNumber = aux.optNumber;
    pub const checkInteger = aux.checkInteger;
    pub const optInteger = aux.optInteger;
    pub const checkInteger64 = aux.checkInteger64;
    pub const optInteger64 = aux.optInteger64;
    pub const checkUnsigned = aux.checkUnsigned;
    pub const optUnsigned = aux.optUnsigned;
    pub const checkBoolean = aux.checkBoolean;
    pub const optBoolean = aux.optBoolean;
    pub const checkVector = aux.checkVector;
    pub const optVector = aux.optVector;
    pub const checkType = aux.checkType;
    pub const checkAny = aux.checkAny;
    pub const checkStackMsg = aux.checkStackMsg;
    pub const checkUserdata = aux.checkUserdata;
    pub const checkBuffer = aux.checkBuffer;
    pub const checkOption = aux.checkOption;
    pub const newMetatable = aux.newMetatable;
    pub const getMetatableNamed = aux.getMetatableNamed;
    pub const getMetafield = aux.getMetafield;
    pub const callMeta = aux.callMeta;
    pub const where = aux.where;
    pub const toStringMeta = aux.toStringMeta;
    pub const findTable = aux.findTable;
    pub const callYieldable = aux.callYieldable;
    pub const pcallYieldable = aux.pcallYieldable;
    pub const register = aux.register;

    // ---- remaining lua_* tail (see misc.zig) -----------------------------
    const misc = @import("misc.zig");
    pub const Destructor = misc.Destructor;
    pub const isInteger64 = misc.isInteger64;
    pub const toStringAtom = misc.toStringAtom;
    pub const toStringAtomZ = misc.toStringAtomZ;
    pub const namecallAtom = misc.namecallAtom;
    pub const toLightUserdataTagged = misc.toLightUserdataTagged;
    pub const toUserdataTagged = misc.toUserdataTagged;
    pub const userdataTag = misc.userdataTag;
    pub const lightUserdataTag = misc.lightUserdataTag;
    pub const newUserdataTagged = misc.newUserdataTagged;
    pub const newUserdata = misc.newUserdata;
    pub const newUserdataWithMetatable = misc.newUserdataWithMetatable;
    pub const newUserdataDtor = misc.newUserdataDtor;
    pub const newBuffer = misc.newBuffer;
    pub const setUserdataTag = misc.setUserdataTag;
    pub const setUserdataDtor = misc.setUserdataDtor;
    pub const getUserdataDtor = misc.getUserdataDtor;
    pub const setUserdataMetatable = misc.setUserdataMetatable;
    pub const getUserdataMetatable = misc.getUserdataMetatable;
    pub const registerUserdataDirectAccess = misc.registerUserdataDirectAccess;
    pub const registerUserdataDirectFieldGet = misc.registerUserdataDirectFieldGet;
    pub const directFieldSetNumber = misc.directFieldSetNumber;
    pub const directFieldSetVector = misc.directFieldSetVector;
    pub const directFieldSetBoolean = misc.directFieldSetBoolean;
    pub const directFieldSetInteger64 = misc.directFieldSetInteger64;
    pub const directFieldSetNil = misc.directFieldSetNil;
    pub const setLightUserdataName = misc.setLightUserdataName;
    pub const getLightUserdataName = misc.getLightUserdataName;
    pub const rawGetP = misc.rawGetP;
    pub const rawSetP = misc.rawSetP;
    pub const rawGetPTagged = misc.rawGetPTagged;
    pub const rawSetPTagged = misc.rawSetPTagged;
    pub const cloneFunction = misc.cloneFunction;
    pub const clearTable = misc.clearTable;
    pub const cloneTable = misc.cloneTable;
    pub const cpcall = misc.cpcall;
    pub const rawIter = misc.rawIter;
    pub const encodePointer = misc.encodePointer;
    pub const pushStringZ = misc.pushStringZ;
    pub const pushFString = misc.pushFString;
    /// Compile, run, and return the first result typed: `try lua.eval(u32, src)`.
    pub fn eval(lua: *Lua, comptime T: type, chunkname: [:0]const u8, source: []const u8) (Error || value.Error)!T {
        try lua.loadString(chunkname, source);
        try lua.pcall(0, 1, 0);
        defer lua.pop(1);
        return value.pull(T, lua, -1);
    }

    /// Save the current stack top; `defer s.restore()` trims back to it (RAII
    /// stack discipline). `var s = lua.scope(); defer s.restore();`
    pub fn scope(lua: *Lua) Scope {
        return .{ .lua = lua, .saved_top = lua.getTop() };
    }
};

/// A saved stack height; `restore()` discards everything pushed since.
pub const Scope = struct {
    lua: *Lua,
    saved_top: i32,
    pub fn restore(self: Scope) void {
        self.lua.setTop(self.saved_top);
    }
};
