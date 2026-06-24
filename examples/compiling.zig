//! Compile Luau source to standalone bytecode, then (where supported) JIT it to
//! native code and inspect the disassembly.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();
    const alloc = gpa.allocator();

    var vm = try luau.Lua.init(alloc);
    defer vm.deinit();
    vm.openLibs();

    // 1) compile to owned bytecode (e.g. to cache or ship precompiled)
    const bc = try luau.compile(alloc, "local function f(a, b) return a + b end\nreturn f(20, 22)", .{
        .optimization_level = 2,
    });
    defer alloc.free(bc);
    std.debug.print("bytecode: {d} bytes\n", .{bc.len});

    try vm.loadBytecode("=chunk", bc, 0);

    // 2) native codegen, if the platform supports it
    if (luau.codegen.supported()) {
        luau.codegen.create(vm);
        var stats: luau.codegen.Stats = undefined;
        _ = luau.codegen.compileWithStats(vm, -1, 0, &stats);
        std.debug.print("jit: {d} functions, {d} bytes of native code\n", .{
            stats.functions_compiled, stats.native_code_size,
        });
        if (luau.codegen.getAssembly(alloc, vm, -1, true, false)) |text| {
            defer alloc.free(text);
            std.debug.print("disassembly: {d} chars\n", .{text.len});
        } else |_| {}
    } else {
        std.debug.print("native codegen not supported here\n", .{});
    }

    // 3) run it
    try vm.pcall(0, 1, 0);
    std.debug.print("result = {d}\n", .{vm.toNumber(-1).?});
}
