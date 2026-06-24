//! Typed registry references — pin a Luau value across stack churn.

const std = @import("std");
const Lua = @import("lua.zig").Lua;
const value = @import("value.zig");
const enums = @import("enums.zig");

/// A registry-pinned reference to a Luau value, typed as `T` for retrieval.
/// Use `Ref(void)` (aka `AnyRef`) when you only need to re-push the value.
pub fn Ref(comptime T: type) type {
    return struct {
        const Self = @This();

        lua: *Lua,
        id: i32,

        /// Pin the value at `idx` (it is *not* popped).
        pub fn init(lua: *Lua, idx: i32) Self {
            lua.pushValue(idx);
            defer lua.pop(1);
            return .{ .lua = lua, .id = lua.ref(-1) };
        }

        /// Pin the value currently on top of the stack (popping it).
        pub fn pop(lua: *Lua) Self {
            defer lua.pop(1);
            return .{ .lua = lua, .id = lua.ref(-1) };
        }

        /// Release the reference.
        pub fn deinit(self: Self) void {
            self.lua.unref(self.id);
        }

        /// Push the referenced value onto the stack.
        pub fn push(self: Self) void {
            _ = self.lua.getRef(self.id);
        }

        /// Retrieve the referenced value marshalled as `T`.
        pub fn get(self: Self) value.Error!T {
            self.push();
            defer self.lua.pop(1);
            return value.pull(T, self.lua, -1);
        }

        /// Whether this is the `nil` reference.
        pub fn isNil(self: Self) bool {
            return self.id == enums.refnil;
        }
    };
}

/// An untyped reference (re-push only).
pub const AnyRef = Ref(void);
