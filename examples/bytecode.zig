//! Advanced: hand-emit Luau bytecode with `BytecodeBuilder`, then load and run
//! it on the VM. (Most users want `luau.compile`; this is the low-level path.)

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    const Op = luau.bytecode.emit.Op;

    // Build a main function equivalent to `return 6 * 7`.
    const b = luau.bytecode.emit.Builder.init();
    defer b.deinit();

    const fid = b.beginFunction(0, false);
    const k = b.addConstantNumber(42);
    b.emitAD(@intFromEnum(Op.loadk), 0, @intCast(k)); // LOADK r0, k(42)
    b.emitABC(@intFromEnum(Op.@"return"), 0, 2, 0); //   RETURN r0, 1 value
    b.endFunction(1, 0); // max stack 1, no upvalues
    b.setMainFunction(fid);
    b.finalize();

    std.debug.print("emitted {d} bytes of bytecode\n", .{b.bytecode().len});

    var vm = try luau.Lua.init(std.heap.page_allocator);
    defer vm.deinit();
    vm.openLibs();

    try vm.loadBytecode("=handmade", b.bytecode(), 0);
    try vm.pcall(0, 1, 0);
    std.debug.print("running it returned: {d}\n", .{vm.toNumber(-1).?});
}
