//! Comptime marshalling of Zig values to/from Luau stack values.

const std = @import("std");
const c = @import("bindings");
const Lua = @import("lua.zig").Lua;
const usertype = @import("usertype.zig");

pub const Error = error{
    /// A stack value could not be converted to the requested Zig type.
    LuaTypeMismatch,
    /// Building a list/array argument from a Luau table ran out of memory.
    OutOfMemory,
};

// ---- push: Zig value -> stack ------------------------------------------------

/// Push any supported Zig value onto the stack as the natural Luau value.
pub fn push(lua: *Lua, val: anytype) void {
    const T = @TypeOf(val);
    switch (@typeInfo(T)) {
        .void, .null => lua.pushNil(),
        .bool => lua.pushBoolean(val),
        .int => |info| {
            if (info.bits <= 32 and info.signedness == .signed) {
                lua.pushInteger(@intCast(val));
            } else if (info.bits <= 53) {
                lua.pushNumber(@floatFromInt(val));
            } else {
                lua.pushInteger64(@intCast(val));
            }
        },
        .comptime_int => lua.pushNumber(@floatFromInt(val)),
        .float, .comptime_float => lua.pushNumber(@floatCast(val)),
        .@"enum" => lua.pushInteger(@intFromEnum(val)),
        .enum_literal => lua.pushString(@tagName(val)),
        .optional => if (val) |v| push(lua, v) else lua.pushNil(),
        .error_union => {
            const v = val catch |err| lua.raiseErrorStr(@errorName(err));
            push(lua, v);
        },
        .pointer => |p| switch (p.size) {
            .slice => if (p.child == u8) lua.pushString(val) else pushArrayTable(lua, val),
            .one => switch (@typeInfo(p.child)) {
                .array => |arr| if (arr.child == u8) lua.pushString(val) else pushArrayTable(lua, val),
                else => lua.pushLightUserdata(@ptrCast(@constCast(val))),
            },
            else => @compileError("cannot push pointer type " ++ @typeName(T)),
        },
        .@"struct" => |s| if (s.is_tuple) {
            inline for (val) |elem| push(lua, elem);
        } else if (usertype.isRegistered(lua, T)) {
            // a registered usertype round-trips as userdata, not a plain table
            usertype.pushInstance(lua, T, val);
        } else {
            pushStructTable(lua, val);
        },
        .array => pushArrayTable(lua, val),
        else => @compileError("cannot push Zig type " ++ @typeName(T)),
    }
}

fn pushStructTable(lua: *Lua, val: anytype) void {
    const fields = @typeInfo(@TypeOf(val)).@"struct".fields;
    lua.createTable(0, fields.len);
    inline for (fields) |f| {
        push(lua, @field(val, f.name));
        lua.setField(-2, f.name ++ "");
    }
}

fn pushArrayTable(lua: *Lua, val: anytype) void {
    lua.createTable(@intCast(val.len), 0);
    for (val, 0..) |elem, i| {
        push(lua, elem);
        lua.rawSetIndex(-2, @intCast(i + 1));
    }
}

// ---- pull: stack -> Zig value ------------------------------------------------

/// Read the value at `idx` as a `T`, or return `error.LuaTypeMismatch`.
pub fn pull(comptime T: type, lua: *Lua, idx: i32) Error!T {
    return switch (@typeInfo(T)) {
        .void => {},
        .bool => lua.toBoolean(idx),
        // Mirror `push`'s int branching so every width round-trips: small signed
        // ints and i64 use the integer accessors; mid-width ints use the number.
        .int => |info| if (info.bits <= 32 and info.signedness == .signed)
            @intCast(lua.toInteger(idx) orelse return error.LuaTypeMismatch)
        else if (info.bits <= 53)
            @intFromFloat(lua.toNumber(idx) orelse return error.LuaTypeMismatch)
        else
            @intCast(lua.toInteger64(idx) orelse return error.LuaTypeMismatch),
        .float => @floatCast(lua.toNumber(idx) orelse return error.LuaTypeMismatch),
        .@"enum" => @enumFromInt(lua.toInteger(idx) orelse return error.LuaTypeMismatch),
        .optional => |o| if (lua.isNoneOrNil(idx)) null else try pull(o.child, lua, idx),
        .pointer => |p| if (p.size == .slice and p.child == u8 and p.is_const)
            (lua.toString(idx) orelse return error.LuaTypeMismatch)
        else if (p.size == .one)
            @ptrCast(@alignCast(lua.toUserdata(idx) orelse return error.LuaTypeMismatch))
        else
            @compileError("cannot pull pointer type " ++ @typeName(T)),
        .array => try pullArray(T, lua, idx),
        .@"struct" => if (usertype.getInstance(lua, T, idx)) |p| p.* else try pullStruct(T, lua, idx),
        else => @compileError("cannot pull Zig type " ++ @typeName(T)),
    };
}

/// Like `pull`, but with an allocator so variable-length lists (non-`u8` slices)
/// can be built from Luau arrays. Used for function arguments, where the caller
/// frees everything after the call (e.g. via an arena).
pub fn pullAlloc(comptime T: type, lua: *Lua, idx: i32, alloc: std.mem.Allocator) Error!T {
    return switch (@typeInfo(T)) {
        .optional => |o| if (lua.isNoneOrNil(idx)) null else try pullAlloc(o.child, lua, idx, alloc),
        .array => try pullArrayAlloc(T, lua, idx, alloc),
        .@"struct" => if (usertype.getInstance(lua, T, idx)) |p| p.* else try pullStructAlloc(T, lua, idx, alloc),
        .pointer => |p| if (p.size == .slice and p.child != u8) blk: {
            if (!lua.isTable(idx)) return error.LuaTypeMismatch;
            const aidx = lua.absIndex(idx);
            const n = lua.objLen(idx);
            const buf = try alloc.alloc(p.child, n);
            for (0..n) |k| {
                _ = lua.rawGetIndex(aidx, @intCast(k + 1));
                buf[k] = pullAlloc(p.child, lua, -1, alloc) catch |e| {
                    lua.pop(1);
                    return e;
                };
                lua.pop(1);
            }
            break :blk buf;
        } else try pull(T, lua, idx),
        else => try pull(T, lua, idx),
    };
}

fn pullArray(comptime T: type, lua: *Lua, idx: i32) Error!T {
    const arr = @typeInfo(T).array;
    if (!lua.isTable(idx)) return error.LuaTypeMismatch;
    const aidx = lua.absIndex(idx);
    var out: T = undefined;
    inline for (0..arr.len) |k| {
        _ = lua.rawGetIndex(aidx, @intCast(k + 1));
        out[k] = pull(arr.child, lua, -1) catch |e| {
            lua.pop(1);
            return e;
        };
        lua.pop(1);
    }
    return out;
}
fn pullArrayAlloc(comptime T: type, lua: *Lua, idx: i32, alloc: std.mem.Allocator) Error!T {
    const arr = @typeInfo(T).array;
    if (!lua.isTable(idx)) return error.LuaTypeMismatch;
    const aidx = lua.absIndex(idx);
    var out: T = undefined;
    inline for (0..arr.len) |k| {
        _ = lua.rawGetIndex(aidx, @intCast(k + 1));
        out[k] = pullAlloc(arr.child, lua, -1, alloc) catch |e| {
            lua.pop(1);
            return e;
        };
        lua.pop(1);
    }
    return out;
}

fn pullStruct(comptime T: type, lua: *Lua, idx: i32) Error!T {
    if (!lua.isTable(idx)) return error.LuaTypeMismatch;
    const aidx = lua.absIndex(idx);
    var result: T = undefined;
    inline for (@typeInfo(T).@"struct".fields) |f| {
        _ = lua.getField(aidx, f.name ++ "");
        @field(result, f.name) = pull(f.type, lua, -1) catch |e| {
            lua.pop(1);
            return e;
        };
        lua.pop(1);
    }
    return result;
}
fn pullStructAlloc(comptime T: type, lua: *Lua, idx: i32, alloc: std.mem.Allocator) Error!T {
    if (!lua.isTable(idx)) return error.LuaTypeMismatch;
    const aidx = lua.absIndex(idx);
    var result: T = undefined;
    inline for (@typeInfo(T).@"struct".fields) |f| {
        _ = lua.getField(aidx, f.name ++ "");
        @field(result, f.name) = pullAlloc(f.type, lua, -1, alloc) catch |e| {
            lua.pop(1);
            return e;
        };
        lua.pop(1);
    }
    return result;
}

// ---- convenience: globals by name -------------------------------------------

/// Set global `name` to any marshalled Zig value.
pub fn set(lua: *Lua, name: [:0]const u8, val: anytype) void {
    push(lua, val);
    lua.setGlobal(name);
}

/// Read global `name` as a `T`.
pub fn get(lua: *Lua, comptime T: type, name: [:0]const u8) Error!T {
    _ = lua.getGlobal(name);
    defer lua.pop(1);
    return pull(T, lua, -1);
}
