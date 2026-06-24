//! Comptime marshalling: values by name, auto-marshalled functions, errors
//! crossing the boundary, and structs <-> tables.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

fn addNums(a: f64, b: f64) f64 {
    return a + b;
}
fn divide(a: f64, b: f64) !f64 {
    if (b == 0) return error.DivideByZero;
    return a / b;
}

test "set/get globals by value" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.set("answer", @as(i32, 42));
    vm.set("name", "luau");
    vm.set("flag", true);
    try testing.expectEqual(@as(i32, 42), try vm.get(i32, "answer"));
    try testing.expectEqualStrings("luau", try vm.get([]const u8, "name"));
    try testing.expectEqual(true, try vm.get(bool, "flag"));
}

test "auto-marshalled function with a real signature" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("add", addNums);
    try testing.expectEqual(@as(f64, 42.0), try vm.eval(f64, "=t", "return add(40, 2)"));
}

test "a returned Zig error becomes a Luau error" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("divide", divide);
    try vm.doString("=t", "ok, err = pcall(divide, 1, 0)");
    try testing.expectEqual(false, try vm.get(bool, "ok"));
    try testing.expect(std.mem.indexOf(u8, try vm.get([]const u8, "err"), "DivideByZero") != null);
}

test "struct <-> table" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    const Point = struct { x: f64, y: f64 };
    vm.set("p", Point{ .x = 1.5, .y = 2.5 });
    try testing.expectEqual(@as(f64, 4.0), try vm.eval(f64, "=t", "return p.x + p.y"));

    try vm.doString("=t", "q = { x = 10, y = 20 }");
    const q = try vm.get(Point, "q");
    try testing.expectEqual(@as(f64, 10), q.x);
    try testing.expectEqual(@as(f64, 20), q.y);
}
