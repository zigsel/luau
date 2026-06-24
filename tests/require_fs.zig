//! The ready-made, sandboxed `FsResolver`: loads modules from a directory and
//! cannot escape it.

const std = @import("std");
const luau = @import("luau");
const Lua = luau.Lua;
const req = luau.require;
const testing = std.testing;
const io = std.testing.io;

test "FsResolver loads modules and jails them to the root" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const root = try std.fmt.allocPrint(testing.allocator, ".zig-cache/tmp/{s}", .{tmp.sub_path});
    defer testing.allocator.free(root);

    try tmp.dir.writeFile(io, .{ .sub_path = "math.luau", .data = "return { add = function(a, b) return a + b end }" });
    try tmp.dir.writeFile(io, .{ .sub_path = "app.luau", .data = "return 'app'" });

    var fs = req.FsResolver.init(io, testing.allocator, .{ .root = root });
    var resolver = fs.resolver();

    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    req.install(vm, &resolver);

    // happy path: a module in the jail loads
    try vm.loadString("=main",
        \\local math = require("./math")
        \\return math.add(40, 2)
    );
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 42), vm.toNumber(-1).?);
    vm.pop(1);

    // jail: escaping the root must fail, not read outside it
    vm.loadString("=main2", "return require(\"../../../etc/hosts\")") catch unreachable;
    try testing.expectError(error.Runtime, vm.pcall(0, 1, 0));
}

fn denyPrivate(path: []const u8) bool {
    return !std.mem.startsWith(u8, path, "private");
}

test "FsResolver allow-hook can deny a subtree" {
    var tmp = testing.tmpDir(.{});
    defer tmp.cleanup();
    const root = try std.fmt.allocPrint(testing.allocator, ".zig-cache/tmp/{s}", .{tmp.sub_path});
    defer testing.allocator.free(root);
    try tmp.dir.writeFile(io, .{ .sub_path = "private.luau", .data = "return 'secret'" });

    var fs = req.FsResolver.init(io, testing.allocator, .{ .root = root, .allow = denyPrivate });
    var resolver = fs.resolver();

    var vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    req.install(vm, &resolver);

    vm.loadString("=main", "return require(\"./private\")") catch unreachable;
    try testing.expectError(error.Runtime, vm.pcall(0, 1, 0)); // denied -> not found
}
