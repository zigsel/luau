//! Embedding: create a state, run a script, read globals back into Zig — typed.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    // run a chunk for its side effects
    try vm.doString("=config",
        \\ name = "luau"
        \\ port = 8080
        \\ debug_mode = true
    );

    // read globals back, each as the Zig type you ask for
    const name = try vm.get([]const u8, "name");
    const port = try vm.get(u16, "port");
    const debug_mode = try vm.get(bool, "debug_mode");
    std.debug.print("name={s} port={d} debug={}\n", .{ name, port, debug_mode });

    // or evaluate an expression and get its result directly
    const sum = try vm.eval(f64, "=expr", "return 2 ^ 10");
    std.debug.print("2^10 = {d}\n", .{sum});
}
