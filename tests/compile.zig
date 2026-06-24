//! The compiler, native codegen (JIT), the bytecode builder, and GC controls.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

test "compiler: compile to owned bytecode, then load it" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();

    const bc = try luau.compile(testing.allocator, "return 6 * 7", .{ .optimization_level = 2 });
    defer testing.allocator.free(bc);
    try testing.expect(!luau.compiler.isErrorBytecode(bc));

    try vm.loadBytecode("=c", bc, 0);
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 42), vm.toNumber(-1).?);
}

test "gc: controls and stats" {
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    vm.gcCollect();
    try testing.expect(vm.gcCount() > 0);
}

test "codegen: JIT-compile with stats and disassemble" {
    if (!luau.codegen.supported()) return;
    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    luau.codegen.create(vm);

    try vm.loadString("=jit", "local function f(a, b) return a + b end\nreturn f(1, 2)");

    var stats: luau.codegen.Stats = undefined;
    const r = luau.codegen.compileWithStats(vm, -1, 0, &stats);
    try testing.expect(r == .success or r == .nothing_to_compile);

    const asm_text = luau.codegen.getAssembly(testing.allocator, vm, -1, true, false) catch "";
    defer if (asm_text.len > 0) testing.allocator.free(asm_text);

    luau.codegen.setNativeExecutionEnabled(vm, true);
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 3), vm.toNumber(-1).?);
}

test "bytecode: hand-build 'return 42' and run it" {
    const Op = luau.bytecode.emit.Op;
    const b = luau.bytecode.emit.Builder.init();
    defer b.deinit();

    const fid = b.beginFunction(0, false);
    const k = b.addConstantNumber(42);
    b.emitAD(@intFromEnum(Op.loadk), 0, @intCast(k)); // LOADK r0, k
    b.emitABC(@intFromEnum(Op.@"return"), 0, 2, 0); // RETURN r0, 1 value
    b.endFunction(1, 0);
    b.setMainFunction(fid);
    b.finalize();

    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    try vm.loadBytecode("=bc", b.bytecode(), 0);
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 42), vm.toNumber(-1).?);
}

test "compile errors: bad source yields error count > 0 with a message" {
    var chk = try luau.compiler.errors.check("local = =", 1, 1);
    defer chk.deinit();
    try testing.expect(chk.count() > 0);
    const e = chk.at(0);
    try testing.expect(e != null);
    try testing.expect(e.?.message.len > 0);
}
