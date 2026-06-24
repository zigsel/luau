//! Expose a whole Zig namespace (functions + constants) as a Luau library table
//! with one call — the metatable is built at comptime.

const std = @import("std");
const luau = @import("luau");

/// Every pub fn becomes a library function; every numeric/bool/string pub const
/// becomes a value.
const mathx = struct {
    pub const pi: f64 = 3.141592653589793;

    pub fn clamp(x: f64, lo: f64, hi: f64) f64 {
        return @min(@max(x, lo), hi);
    }
    pub fn lerp(a: f64, b: f64, t: f64) f64 {
        return a + (b - a) * t;
    }
};

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    vm.bindModule("mathx", mathx);

    try vm.doString("=demo",
        \\ print(mathx.pi)               -- 3.14159...
        \\ print(mathx.clamp(15, 0, 10)) -- 10
        \\ print(mathx.lerp(0, 100, 0.25)) -- 25
    );
}
