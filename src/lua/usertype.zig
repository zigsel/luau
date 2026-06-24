//! Sol-style usertypes: bind a Zig struct as Luau userdata with a constructor,
//! methods, field get/set, metamethods, and an optional destructor — all wired
//! at comptime. Also `bindModule`, which exposes a Zig namespace as a library.

const std = @import("std");
const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const value = @import("value.zig");
const function = @import("function.zig");

/// The metatable registry key for `T` (its fully-qualified name; unique).
fn metatableName(comptime T: type) [:0]const u8 {
    return @typeName(T);
}

/// The short, user-facing name for `T` (final path component).
pub fn shortName(comptime T: type) [:0]const u8 {
    const full = @typeName(T);
    const start = comptime if (std.mem.lastIndexOfScalar(u8, full, '.')) |i| i + 1 else 0;
    return full[start.. :0];
}

const metamethod_names = [_][:0]const u8{
    "__add",    "__sub", "__mul",      "__div",  "__mod",  "__pow",
    "__unm",    "__idiv", "__concat",  "__len",  "__eq",   "__lt",
    "__le",     "__call", "__tostring", "__iter", "__index", "__newindex",
};

fn isMetamethodName(comptime name: []const u8) bool {
    inline for (metamethod_names) |m| {
        if (std.mem.eql(u8, name, m)) return true;
    }
    return false;
}

fn isReservedName(comptime name: []const u8) bool {
    return std.mem.eql(u8, name, "init") or std.mem.eql(u8, name, "deinit") or
        std.mem.eql(u8, name, "new") or isMetamethodName(name);
}

/// Allocate a `T` instance as Luau userdata (with `T`'s registered metatable and
/// destructor) and copy `val` into it. The userdata is left on the stack.
pub fn pushInstance(lua: *Lua, comptime T: type, val: T) void {
    const data = if (@hasDecl(T, "deinit"))
        c.lua_newuserdatadtor(lua.toRaw(), @sizeOf(T), &dtorTrampoline(T))
    else
        c.lua_newuserdatatagged(lua.toRaw(), @sizeOf(T), 0);
    const ptr: *T = @ptrCast(@alignCast(data.?));
    ptr.* = val;
    // attach the registered metatable
    _ = c.luaL_getmetatable(lua.toRaw(), metatableName(T).ptr);
    _ = c.lua_setmetatable(lua.toRaw(), -2);
}

fn dtorTrampoline(comptime T: type) fn (?*anyopaque) callconv(.c) void {
    return struct {
        fn dtor(p: ?*anyopaque) callconv(.c) void {
            const self: *T = @ptrCast(@alignCast(p.?));
            self.deinit();
        }
    }.dtor;
}

/// If the value at `idx` is a `T` instance (its metatable matches), return a
/// pointer to it; otherwise null.
pub fn getInstance(lua: *Lua, comptime T: type, idx: i32) ?*T {
    const data = c.lua_touserdata(lua.toRaw(), idx) orelse return null;
    if (c.lua_getmetatable(lua.toRaw(), idx) == 0) return null; // pushes its mt
    _ = c.luaL_getmetatable(lua.toRaw(), metatableName(T).ptr); // pushes registered mt
    const same = c.lua_rawequal(lua.toRaw(), -1, -2) != 0;
    lua.pop(2);
    if (!same) return null;
    return @ptrCast(@alignCast(data));
}

/// True if `T` has a registered metatable in this state.
pub fn isRegistered(lua: *Lua, comptime T: type) bool {
    const ty = c.luaL_getmetatable(lua.toRaw(), metatableName(T).ptr);
    lua.pop(1);
    return ty != c.LUA_TNIL;
}

// ---- method / metamethod / accessor closures --------------------------------

fn methodClosure(comptime T: type, comptime func: anytype) lua_mod.CFn {
    return struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const F = @TypeOf(func);
            const fi = @typeInfo(F).@"fn";
            var args: std.meta.ArgsTuple(F) = undefined;
            inline for (fi.params, 0..) |p, i| {
                if (i == 0) {
                    const self = getInstance(lua, T, 1) orelse
                        lua.typeError(1, shortName(T));
                    args[0] = switch (p.type.?) {
                        *T, *const T => self,
                        T => self.*,
                        else => @compileError("first method param of " ++ @typeName(T) ++
                            " must be T, *T or *const T"),
                    };
                } else {
                    const stack_idx: i32 = @intCast(i + 1);
                    args[i] = value.pull(p.type.?, lua, stack_idx) catch |e|
                        lua.raiseError("bad argument #{d}: {s}", .{ stack_idx, @errorName(e) });
                }
            }
            return function.pushReturn(lua, @call(.auto, func, args));
        }
    }.call;
}

fn constructorClosure(comptime T: type) lua_mod.CFn {
    return struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const instance: T = build(lua);
            pushInstance(lua, T, instance);
            return 1;
        }
        fn build(lua: *Lua) T {
            if (@hasDecl(T, "init")) {
                const F = @TypeOf(@field(T, "init"));
                const fi = @typeInfo(F).@"fn";
                var args: std.meta.ArgsTuple(F) = undefined;
                inline for (fi.params, 0..) |p, i| {
                    const stack_idx: i32 = @intCast(i + 1);
                    args[i] = value.pull(p.type.?, lua, stack_idx) catch |e|
                        lua.raiseError("bad argument #{d}: {s}", .{ stack_idx, @errorName(e) });
                }
                const r = @call(.auto, @field(T, "init"), args);
                return if (@typeInfo(@TypeOf(r)) == .error_union)
                    (r catch |e| lua.raiseErrorStr(@errorName(e)))
                else
                    r;
            } else {
                // positional construction from fields
                var instance: T = undefined;
                inline for (@typeInfo(T).@"struct".fields, 0..) |f, i| {
                    const stack_idx: i32 = @intCast(i + 1);
                    @field(instance, f.name) = value.pull(f.type, lua, stack_idx) catch |e|
                        lua.raiseError("bad argument #{d}: {s}", .{ stack_idx, @errorName(e) });
                }
                return instance;
            }
        }
    }.call;
}

fn indexClosure(comptime T: type) lua_mod.CFn {
    return struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const self = getInstance(lua, T, 1) orelse lua.typeError(1, shortName(T));
            const key = lua.toString(2) orelse return 0;
            // field read
            inline for (@typeInfo(T).@"struct".fields) |f| {
                if (std.mem.eql(u8, key, f.name)) {
                    value.push(lua, @field(self.*, f.name));
                    return 1;
                }
            }
            // method lookup: methods live in the metatable's "__methods" subtable
            _ = c.luaL_getmetatable(lua.toRaw(), metatableName(T).ptr);
            _ = lua.getField(-1, "__methods");
            lua.pushValue(2);
            _ = lua.rawGet(-2);
            return 1;
        }
    }.call;
}

fn newIndexClosure(comptime T: type) lua_mod.CFn {
    return struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const self = getInstance(lua, T, 1) orelse lua.typeError(1, shortName(T));
            const key = lua.toString(2) orelse lua.raiseErrorStr("invalid field key");
            inline for (@typeInfo(T).@"struct".fields) |f| {
                if (std.mem.eql(u8, key, f.name)) {
                    @field(self.*, f.name) = value.pull(f.type, lua, 3) catch |e|
                        lua.raiseError("cannot set field '{s}': {s}", .{ f.name, @errorName(e) });
                    return 0;
                }
            }
            lua.raiseError("'{s}' is not a field of {s}", .{ key, shortName(T) });
        }
    }.call;
}

/// Register `T` as a usertype: installs its metatable and creates a global
/// constructor table `<ShortName>` with `.new(...)`.
pub fn registerType(lua: *Lua, comptime T: type) void {
    if (@typeInfo(T) != .@"struct") @compileError("registerType expects a struct, got " ++ @typeName(T));

    // metatable
    _ = c.luaL_newmetatable(lua.toRaw(), metatableName(T).ptr);
    const mt = lua.getTop();

    // methods subtable
    lua.newTable();
    inline for (@typeInfo(T).@"struct".decls) |d| {
        const D = @TypeOf(@field(T, d.name));
        if (@typeInfo(D) == .@"fn" and !comptime isReservedName(d.name)) {
            lua.pushCFunction(methodClosure(T, @field(T, d.name)), d.name ++ "");
            lua.setField(-2, d.name ++ "");
        }
    }
    lua.setField(mt, "__methods");

    // user metamethods (operators, __tostring, …)
    inline for (@typeInfo(T).@"struct".decls) |d| {
        if (comptime isMetamethodName(d.name) and
            !std.mem.eql(u8, d.name, "__index") and !std.mem.eql(u8, d.name, "__newindex"))
        {
            lua.pushCFunction(methodClosure(T, @field(T, d.name)), d.name ++ "");
            lua.setField(mt, d.name ++ "");
        }
    }

    // field access (unless the user supplied their own __index/__newindex)
    if (!@hasDecl(T, "__index")) {
        lua.pushCFunction(indexClosure(T), "__index");
        lua.setField(mt, "__index");
    }
    if (!@hasDecl(T, "__newindex")) {
        lua.pushCFunction(newIndexClosure(T), "__newindex");
        lua.setField(mt, "__newindex");
    }
    lua.pop(1); // metatable

    // global constructor table: <ShortName>.new(...)
    lua.newTable();
    lua.pushCFunction(constructorClosure(T), "new");
    lua.setField(-2, "new");
    lua.setGlobal(shortName(T));
}

/// Expose a Zig namespace `T` as a Luau library table named `name`:
/// every `pub fn` becomes an (auto-marshalled) function and every numeric/bool/
/// string `pub const` becomes a value.
pub fn bindModule(lua: *Lua, name: [:0]const u8, comptime T: type) void {
    lua.newTable();
    inline for (@typeInfo(T).@"struct".decls) |d| {
        const field = @field(T, d.name);
        const D = @TypeOf(field);
        switch (@typeInfo(D)) {
            .@"fn" => {
                lua.pushFunction(field, d.name ++ "");
                lua.setField(-2, d.name ++ "");
            },
            .int, .comptime_int, .float, .comptime_float, .bool => {
                value.push(lua, field);
                lua.setField(-2, d.name ++ "");
            },
            else => {}, // skip types, etc.
        }
    }
    lua.setGlobal(name);
}
