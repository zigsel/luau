//! Driving a Luau coroutine from Zig: newThread + load + resume across yields.
//! (Regression: `resumeThread` was previously never exercised and miscompiled.)

const std = @import("std");
const luau = @import("luau");
const testing = std.testing;

test "resume a yielding coroutine to completion" {
    var vm = try luau.Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    const co = vm.newThread();
    try co.loadString("=worker",
        \\for i = 1, 3 do
        \\    coroutine.yield(i * 10)
        \\end
        \\return 999
    );

    var yields: usize = 0;
    var last: f64 = 0;
    while (true) {
        const status = try co.resumeThread(vm, 0);
        last = co.toNumber(-1).?;
        co.pop(1);
        if (status == .ok) break;
        yields += 1;
        try testing.expectEqual(@as(f64, @floatFromInt(yields * 10)), last);
    }
    try testing.expectEqual(@as(usize, 3), yields);
    try testing.expectEqual(@as(f64, 999), last);
}
