//! Capture closures (`setCapture`) and complex/list arguments to marshalled
//! functions (structs, nested structs, slices, fixed arrays).

const std = @import("std");
const luau = @import("luau");
const Lua = luau.Lua;
const testing = std.testing;

fn addScore(score: *i32, n: i32) i32 {
    score.* += n;
    return score.*;
}

test "setCapture: a Zig function bundled with captured state" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    var score: i32 = 0;
    vm.setCapture("addScore", &score, addScore);

    _ = try vm.eval(i32, "=s", "addScore(10); return addScore(5)");
    try testing.expectEqual(@as(i32, 15), score); // the Zig variable was mutated
}

fn sumList(xs: []const i32) i64 {
    var total: i64 = 0;
    for (xs) |x| total += x;
    return total;
}

test "list argument: a Luau array -> Zig slice" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("sumList", sumList);
    const r = try vm.eval(i64, "=l", "return sumList({ 1, 2, 3, 4, 5 })");
    try testing.expectEqual(@as(i64, 15), r);
}

const Vec2 = struct { x: f64, y: f64 };
const Poly = struct { name: []const u8, points: []const Vec2 };

fn perimeterName(p: Poly) f64 {
    _ = p.name;
    var sum: f64 = 0;
    for (p.points) |pt| sum += @abs(pt.x) + @abs(pt.y);
    return sum;
}

test "nested complex argument: struct with a slice of structs" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("perimeterName", perimeterName);
    const r = try vm.eval(f64, "=p",
        \\return perimeterName({
        \\    name = "tri",
        \\    points = { { x = 1, y = 2 }, { x = 3, y = 4 } },
        \\})
    );
    try testing.expectEqual(@as(f64, 10), r); // 1+2+3+4
}

fn sum3(a: [3]i32) i32 {
    return a[0] + a[1] + a[2];
}

test "fixed-array argument" {
    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("sum3", sum3);
    try testing.expectEqual(@as(i32, 6), try vm.eval(i32, "=a", "return sum3({ 1, 2, 3 })"));
}
