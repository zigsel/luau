//! Sol-style usertypes: register a Zig struct and use it from Luau with a
//! constructor, methods, field access, and an operator overload.

const std = @import("std");
const luau = @import("luau");

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

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    vm.registerType(Vec2); // <- binds ctor, methods, fields, operators

    try vm.doString("=demo",
        \\ v = Vec2.new(3, 4)                -- global, so Zig can read it back
        \\ print("length:", v:length())     -- 5
        \\ v:scale(2); v.y = 99
        \\ print("after:", v.x, v.y)         -- 6  99
        \\ local w = Vec2.new(1, 1) + Vec2.new(2, 2)
        \\ print("sum.x:", w.x)              -- 3
    );

    // a registered usertype round-trips back into Zig as a real *Vec2
    _ = vm.getGlobal("v");
    const v = vm.getInstance(Vec2, -1).?;
    std.debug.print("from Zig: v.x={d}\n", .{v.x});
}
