//! Sol-style usertypes and module binding.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

const Vec2 = struct {
    x: f32,
    y: f32,
    pub fn init(x: f32, y: f32) Vec2 {
        return .{ .x = x, .y = y };
    }
    pub fn length(self: Vec2) f32 {
        return @sqrt(self.x * self.x + self.y * self.y);
    }
    pub fn scale(self: *Vec2, k: f32) void {
        self.x *= k;
        self.y *= k;
    }
    pub fn __add(a: Vec2, b: Vec2) Vec2 {
        return .{ .x = a.x + b.x, .y = a.y + b.y };
    }
};

test "usertype: constructor, methods, fields, operators" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    vm.registerType(Vec2);

    try testing.expectEqual(@as(f64, 5), try vm.eval(f64, "=t", "return Vec2.new(3, 4):length()"));
    try testing.expectEqual(@as(f64, 3), try vm.eval(f64, "=t", "return Vec2.new(3, 4).x"));

    // operator overload returns a new userdata; read its field back
    try testing.expectEqual(@as(f64, 4), try vm.eval(f64, "=t",
        "local v = Vec2.new(3,4) + Vec2.new(1,1); return v.x"));

    // mutating method + field write
    try vm.doString("=t", "g = Vec2.new(1, 2); g:scale(10); g.y = 99");
    try testing.expectEqual(@as(f64, 10), try vm.eval(f64, "=t", "return g.x"));
    try testing.expectEqual(@as(f64, 99), try vm.eval(f64, "=t", "return g.y"));
}

const MathLib = struct {
    pub const tau: f64 = 6.283185307179586;
    pub fn square(x: f64) f64 {
        return x * x;
    }
    pub fn hypot(a: f64, b: f64) f64 {
        return @sqrt(a * a + b * b);
    }
};

test "bindModule: a Zig namespace as a Luau library" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    vm.bindModule("m", MathLib);

    try testing.expectEqual(@as(f64, 9), try vm.eval(f64, "=t", "return m.square(3)"));
    try testing.expectEqual(@as(f64, 5), try vm.eval(f64, "=t", "return m.hypot(3, 4)"));
    try testing.expectEqual(@as(f64, 6.283185307179586), try vm.eval(f64, "=t", "return m.tau"));
}
