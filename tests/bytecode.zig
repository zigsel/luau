//! Hand-emitting bytecode with `BytecodeBuilder`.

const std = @import("std");
const luau = @import("luau");
const testing = std.testing;

test "bytecode: build a finalized function, assert non-empty bytecode" {
    const b = luau.bytecode.emit.Builder.init();
    defer b.deinit();

    const fid = b.beginFunction(0, false);
    const ki = b.addConstantInteger(42);
    const kt = b.addConstantTable(&.{ki}, null);
    _ = kt;
    const kk = b.addConstantNumber(1.0);
    b.emitABC(@intFromEnum(luau.bytecode.emit.Op.loadk), 0, @intCast(@as(u32, @bitCast(kk)) & 0xff), 0);
    b.emitABC(@intFromEnum(luau.bytecode.emit.Op.@"return"), 0, 1, 0);
    b.endFunction(1, 0);
    b.setMainFunction(fid);
    b.finalize();

    try testing.expect(b.bytecode().len > 0);
}
