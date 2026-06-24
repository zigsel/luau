//! Embedding the VM: state lifecycle, running scripts, the stack, values,
//! tables, scopes and references.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

test "init with a Zig allocator, run a script, read a global" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    try vm.doString("=t", "result = 6 * 7");
    try testing.expectEqual(@as(f64, 42.0), try vm.get(f64, "result"));
    try testing.expectEqual(@as(i32, 0), vm.getTop());
}

test "typed eval" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    try testing.expectEqual(@as(u32, 2), try vm.eval(u32, "=e", "return 1 + 1"));
    try testing.expectEqualStrings("hi", try vm.eval([]const u8, "=e", "return 'hi'"));
}

test "push and inspect every basic value type" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();

    vm.pushNil();
    vm.pushBoolean(true);
    vm.pushInteger(123);
    vm.pushNumber(3.5);
    vm.pushString("hello\x00world"); // embedded NUL preserved

    try testing.expectEqual(luau.LuaType.nil, vm.typeOf(1));
    try testing.expectEqual(true, vm.toBoolean(2));
    try testing.expectEqual(@as(i32, 123), vm.toInteger(3).?);
    try testing.expectEqual(@as(f64, 3.5), vm.toNumber(4).?);
    try testing.expectEqualStrings("hello\x00world", vm.toString(5).?);
    try testing.expectEqual(@as(usize, 11), vm.objLen(5));
}

test "raw table access and next() iteration" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();

    vm.newTable();
    vm.pushInteger(10);
    vm.setField(-2, "x");
    vm.pushInteger(20);
    vm.setField(-2, "y");

    try testing.expectEqual(luau.LuaType.number, vm.getField(-1, "x"));
    try testing.expectEqual(@as(i32, 10), vm.toInteger(-1).?);
    vm.pop(1);

    var count: usize = 0;
    vm.pushNil();
    while (vm.next(-2)) : (count += 1) vm.pop(1);
    try testing.expectEqual(@as(usize, 2), count);
}

test "scope: RAII stack discipline" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();

    try testing.expectEqual(@as(i32, 0), vm.getTop());
    {
        var s = vm.scope();
        defer s.restore();
        vm.pushInteger(1);
        vm.pushInteger(2);
        vm.pushInteger(3);
        try testing.expectEqual(@as(i32, 3), vm.getTop());
    }
    try testing.expectEqual(@as(i32, 0), vm.getTop());
}

test "Ref: pin a value across stack churn" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    vm.pushString("pinned");
    const r = luau.Ref([]const u8).pop(vm);
    defer r.deinit();

    vm.setTop(0); // churn the stack
    try testing.expectEqualStrings("pinned", try r.get());
}

test "Table handle: get/set/len/iterate" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    var t = luau.Table.init(vm);
    defer t.deinit();
    t.set("a", @as(i32, 1));
    t.set("b", @as(i32, 2));
    t.set("name", "luau");

    try testing.expectEqual(@as(i32, 1), try t.get(i32, "a"));
    try testing.expectEqualStrings("luau", try t.get([]const u8, "name"));

    var sum: i64 = 0;
    var keys: usize = 0;
    var it = t.iterator();
    while (it.next()) {
        if (vm.typeOf(-1) == .number) sum += vm.toInteger(-1).?;
        keys += 1;
        it.step();
    }
    try testing.expectEqual(@as(i64, 3), sum);
    try testing.expectEqual(@as(usize, 3), keys);
}
