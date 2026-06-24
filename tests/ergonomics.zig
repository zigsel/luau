//! sol2-style conveniences: variant (tagged-union) marshalling, get_or, nested
//! table get/set paths, function overloading, and usertype properties.

const std = @import("std");
const luau = @import("luau");
const Lua = luau.Lua;
const testing = std.testing;

// ---- variant (tagged union) marshalling ------------------------------------

const Scalar = union(enum) {
    number: f64,
    text: []const u8,
    flag: bool,
};

fn describe(s: Scalar) []const u8 {
    return switch (s) {
        .number => "number",
        .text => "text",
        .flag => "flag",
    };
}

test "variant: a tagged union marshals to/from the matching Luau value" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("describe", describe);
    try testing.expectEqualStrings("number", try vm.eval([]const u8, "=v", "return describe(42)"));
    try testing.expectEqualStrings("text", try vm.eval([]const u8, "=v", "return describe('hi')"));
    try testing.expectEqualStrings("flag", try vm.eval([]const u8, "=v", "return describe(true)"));
}

// ---- get_or + nested table get/set paths -----------------------------------

test "get_or returns a default for missing/mismatched globals" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    try vm.doString("=g", "port = 8080");
    try testing.expectEqual(@as(u16, 8080), vm.getOr(u16, "port", 1));
    try testing.expectEqual(@as(u16, 1), vm.getOr(u16, "missing", 1));
}

test "nested table get/set paths via globals()" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    var g = vm.globals();
    defer g.deinit();

    // create config.window.width = 1920 from scratch
    g.setPath(&.{ "config", "window", "width" }, @as(i32, 1920));
    try testing.expectEqual(@as(i32, 1920), try g.getPath(i32, &.{ "config", "window", "width" }));
    // visible to Luau, too
    try testing.expectEqual(@as(i32, 1920), try vm.eval(i32, "=n", "return config.window.width"));
}

// ---- overloading ------------------------------------------------------------

fn greet0() []const u8 {
    return "hello";
}
fn greet1(name: []const u8) []const u8 {
    _ = name;
    return "hello, you";
}

test "overload: dispatch by argument count" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setOverload("greet", .{ greet0, greet1 });
    try testing.expectEqualStrings("hello", try vm.eval([]const u8, "=o", "return greet()"));
    try testing.expectEqualStrings("hello, you", try vm.eval([]const u8, "=o", "return greet('bob')"));
}

// ---- usertype properties ----------------------------------------------------

const Circle = struct {
    radius: f64,

    pub fn init(r: f64) Circle {
        return .{ .radius = r };
    }

    // computed properties: `area` (read-only) and `diameter` (read/write).
    pub const properties = .{
        .area = .{ .get = getArea },
        .diameter = .{ .get = getDiameter, .set = setDiameter },
    };
    fn getArea(self: *const Circle) f64 {
        return 3.14159 * self.radius * self.radius;
    }
    fn getDiameter(self: *const Circle) f64 {
        return self.radius * 2;
    }
    fn setDiameter(self: *Circle, d: f64) void {
        self.radius = d / 2;
    }
};

test "usertype: computed properties (read-only + read/write)" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    vm.registerType(Circle);

    // read-only `area`, and a `diameter` setter that updates `radius`
    const r = try vm.eval(f64, "=p",
        \\local c = Circle.new(10)
        \\assert(math.abs(c.area - 314.159) < 0.01)
        \\c.diameter = 40        -- setter -> radius = 20
        \\return c.radius
    );
    try testing.expectEqual(@as(f64, 20), r);
}
