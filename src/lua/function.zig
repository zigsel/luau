//! Comptime wrapping of Zig functions into Luau-callable closures, with
//! automatic argument and result marshalling (errors → Luau errors).

const std = @import("std");
const c = @import("bindings");
const lua_mod = @import("lua.zig");
const Lua = lua_mod.Lua;
const value = @import("value.zig");

fn returnsResultCount(comptime R: type) bool {
    const Payload = if (@typeInfo(R) == .error_union) @typeInfo(R).error_union.payload else R;
    return Payload == void or @typeInfo(Payload) == .int;
}

fn isRawStyle(comptime F: type) bool {
    const fi = @typeInfo(F).@"fn";
    return fi.params.len == 1 and fi.params[0].type == *Lua and
        returnsResultCount(fi.return_type.?);
}

/// Wrap a Zig function as a raw `lua_CFunction`.
///
/// Two styles, auto-detected:
///   * **raw**: `fn(*Lua) c_int` (optionally `!c_int`/`void`) — you manage the
///     stack and return the result count.
///   * **marshalled**: any other signature — arguments are pulled from the stack
///     by type, result(s) pushed back, a returned `error` raised. An optional
///     leading `*Lua` parameter receives the state.
pub fn wrap(comptime func: anytype) lua_mod.CFn {
    const Wrapped = struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            if (comptime isRawStyle(@TypeOf(func))) {
                const R = @typeInfo(@TypeOf(func)).@"fn".return_type.?;
                if (@typeInfo(R) == .error_union) {
                    const v = func(lua) catch |err| lua.raiseErrorStr(@errorName(err));
                    return resultCount(v);
                }
                return resultCount(func(lua));
            } else {
                return callMarshalled(func, lua);
            }
        }
    };
    return Wrapped.call;
}

inline fn resultCount(v: anytype) c_int {
    return switch (@typeInfo(@TypeOf(v))) {
        .void => 0,
        .int, .comptime_int => @intCast(v),
        else => @compileError("raw Luau function must return an integer result count or void"),
    };
}

/// True if any of `F`'s params marshal a variable-length list (so the call needs
/// an allocator). `skip` ignores a leading injected param (`*Lua` or a context).
fn anyParamAllocates(comptime F: type, comptime skip: usize) bool {
    const fi = @typeInfo(F).@"fn";
    inline for (fi.params, 0..) |p, i| {
        if (i < skip) continue;
        if (typeAllocates(p.type.?)) return true;
    }
    return false;
}
fn typeAllocates(comptime T: type) bool {
    return switch (@typeInfo(T)) {
        .pointer => |p| p.size == .slice and p.child != u8,
        .optional => |o| typeAllocates(o.child),
        .array => |a| typeAllocates(a.child),
        .@"struct" => |s| blk: {
            inline for (s.fields) |f| if (typeAllocates(f.type)) break :blk true;
            break :blk false;
        },
        else => false,
    };
}

fn callMarshalled(comptime func: anytype, lua: *Lua) c_int {
    const F = @TypeOf(func);
    const fi = @typeInfo(F).@"fn";
    const has_state = fi.params.len > 0 and fi.params[0].type == *Lua;
    const skip: usize = if (has_state) 1 else 0;

    // Only spin up an arena (freed after the call) if a param needs allocation.
    const needs_alloc = comptime anyParamAllocates(F, skip);
    var arena: std.heap.ArenaAllocator = if (needs_alloc) .init(lua.allocator()) else undefined;
    defer if (needs_alloc) arena.deinit();

    var args: std.meta.ArgsTuple(F) = undefined;
    inline for (fi.params, 0..) |p, i| {
        if (has_state and i == 0) {
            args[0] = lua;
        } else {
            const stack_idx: i32 = @intCast(i + (if (has_state) 0 else 1));
            args[i] = (if (needs_alloc)
                value.pullAlloc(p.type.?, lua, stack_idx, arena.allocator())
            else
                value.pull(p.type.?, lua, stack_idx)) catch |e|
                lua.raiseError("bad argument #{d}: {s}", .{ stack_idx, @errorName(e) });
        }
    }
    return pushReturn(lua, @call(.auto, func, args));
}

pub fn pushReturn(lua: *Lua, ret: anytype) c_int {
    const R = @TypeOf(ret);
    switch (@typeInfo(R)) {
        .error_union => {
            const v = ret catch |err| lua.raiseErrorStr(@errorName(err));
            return pushReturn(lua, v);
        },
        .void => return 0,
        .@"struct" => |s| if (s.is_tuple) {
            inline for (ret) |elem| value.push(lua, elem);
            return @intCast(s.fields.len);
        } else {
            value.push(lua, ret);
            return 1;
        },
        else => {
            value.push(lua, ret);
            return 1;
        },
    }
}

/// Register a Zig function as global `name` (auto-marshalled).
pub fn setFn(lua: *Lua, name: [:0]const u8, comptime func: anytype) void {
    lua.pushFunction(func, name);
    lua.setGlobal(name);
}

// ---- overloading: dispatch same-named functions by argument count -----------

fn luauArity(comptime F: type) c_int {
    const fi = @typeInfo(F).@"fn";
    const has_state = fi.params.len > 0 and fi.params[0].type == *Lua;
    return @intCast(fi.params.len - @intFromBool(has_state));
}

/// Combine several functions into one `lua_CFunction` that dispatches on the
/// number of arguments at the call site (sol2 `overload`). `funcs` is a tuple of
/// functions, e.g. `overload(.{ zero, one, two })`.
pub fn overload(comptime funcs: anytype) lua_mod.CFn {
    const fields = @typeInfo(@TypeOf(funcs)).@"struct".fields;
    const Wrapped = struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const top = lua.getTop();
            inline for (fields) |fld| {
                const f = @field(funcs, fld.name);
                if (top == comptime luauArity(@TypeOf(f))) return callMarshalled(f, lua);
            }
            return lua.raiseError("no overload takes {d} argument(s)", .{top});
        }
    };
    return Wrapped.call;
}

/// Register an overloaded function as global `name`.
pub fn setOverload(lua: *Lua, name: [:0]const u8, comptime funcs: anytype) void {
    lua.pushCFunction(overload(funcs), name);
    lua.setGlobal(name);
}

// ---- capture closures: a Zig function bundled with a captured value ---------

fn callMarshalledCtx(comptime func: anytype, lua: *Lua, ctx: anytype) c_int {
    const F = @TypeOf(func);
    const fi = @typeInfo(F).@"fn";

    const needs_alloc = comptime anyParamAllocates(F, 1); // param 0 is the context
    var arena: std.heap.ArenaAllocator = if (needs_alloc) .init(lua.allocator()) else undefined;
    defer if (needs_alloc) arena.deinit();

    var args: std.meta.ArgsTuple(F) = undefined;
    args[0] = ctx;
    inline for (fi.params, 0..) |p, i| {
        if (i == 0) continue;
        const stack_idx: i32 = @intCast(i); // Luau arg 1 → stack 1
        args[i] = (if (needs_alloc)
            value.pullAlloc(p.type.?, lua, stack_idx, arena.allocator())
        else
            value.pull(p.type.?, lua, stack_idx)) catch |e|
            lua.raiseError("bad argument #{d}: {s}", .{ stack_idx, @errorName(e) });
    }
    return pushReturn(lua, @call(.auto, func, args));
}

/// Wrap `func` (whose first parameter is a captured context of type `Ctx`) as a
/// `lua_CFunction`. The context is read from upvalue 1.
pub fn wrapCapture(comptime Ctx: type, comptime func: anytype) lua_mod.CFn {
    const Wrapped = struct {
        fn call(s: ?*c.lua_State) callconv(.c) c_int {
            const lua = Lua.fromRaw(s.?);
            const ctx: Ctx = @ptrCast(@alignCast(lua.toLightUserdata(Lua.upvalueIndex(1)).?));
            return callMarshalledCtx(func, lua, ctx);
        }
    };
    return Wrapped.call;
}

/// Register a *closure*: `func` bundled with a captured value `ctx`. `func`'s
/// first parameter receives `ctx`; the rest are marshalled from Luau, exactly
/// like `setFn`. `ctx` must be a single-item pointer and must outlive the VM.
///
///     fn addScore(score: *i32, n: i32) i32 { score.* += n; return score.*; }
///     vm.setCapture("addScore", &my_score, addScore);  // Luau: addScore(10)
pub fn setCapture(lua: *Lua, name: [:0]const u8, ctx: anytype, comptime func: anytype) void {
    const Ctx = @TypeOf(ctx);
    comptime {
        if (@typeInfo(Ctx) != .pointer) @compileError("setCapture ctx must be a pointer, got " ++ @typeName(Ctx));
        const first = @typeInfo(@TypeOf(func)).@"fn".params[0].type.?;
        if (first != Ctx) @compileError("setCapture: function's first parameter must be " ++ @typeName(Ctx) ++ ", got " ++ @typeName(first));
    }
    lua.pushLightUserdata(@constCast(ctx));
    lua.pushCClosure(wrapCapture(Ctx, func), name, 1, null);
    lua.setGlobal(name);
}
