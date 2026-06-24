//! CodeGen: the x64 / a64 assemblers and the IR builder. Tolerant — we assert
//! "non-empty code" / "accessors don't crash" rather than exact bytes, which
//! vary by host and Luau version.

const std = @import("std");
const luau = @import("luau");

const x64 = luau.codegen.x64;
const a64 = luau.codegen.a64;
const ir = luau.codegen.ir;

test "x64: build a tiny function (mov rax, imm; ret) and finalize" {
    var a = try x64.Assembler.init(false, 0);
    defer a.deinit();

    const rax = x64.reg.rax();
    defer rax.deinit();

    a.mov64(rax, 42);
    a.ret();

    try std.testing.expect(a.finalize());
    try std.testing.expect(a.code().len > 0);
    try std.testing.expect(a.codeSize() > 0);
}

test "x64: operand/immediate plumbing emits code" {
    var a = try x64.Assembler.init(false, 0);
    defer a.deinit();

    const rax = x64.reg.rax();
    defer rax.deinit();
    const rax_op = rax.op();
    defer rax_op.deinit();
    const imm = x64.Operand.imm(7);
    defer imm.deinit();

    a.add(rax_op, imm);
    a.ret();

    try std.testing.expect(a.finalize());
    try std.testing.expect(a.code().len > 0);
}

test "x64: emit ret via the SystemV ABI constructor" {
    var a = try x64.Assembler.initAbi(false, .systemv, 0);
    defer a.deinit();

    a.ret();
    try std.testing.expect(a.finalize());
    try std.testing.expect(a.code().len > 0);
}

test "a64: build a tiny function (mov + ret) and finalize" {
    var b = try a64.Builder.init(false, 0);
    defer b.deinit();

    const x0 = try a64.Reg.init(.x, 0);
    defer x0.deinit();
    const x1 = try a64.Reg.init(.x, 1);
    defer x1.deinit();

    b.mov(x0, x1);
    b.movImm(x0, 5);
    b.ret();

    try b.finalize();

    const code = b.code();
    try std.testing.expect(code.len > 0);
    try std.testing.expect(code.len % 4 == 0);
    try std.testing.expect(b.codeSize() > 0);
}

test "ir: construct constants/ops and run inspection accessors" {
    var b = try ir.Builder.init();
    defer b.deinit();

    const ci = b.constInt(11);
    const cd = b.constDouble(2.5);
    const ct = b.constTag(3);

    _ = ci.kind();
    _ = ci.index();
    _ = cd.kind();
    _ = ct.kind();

    try std.testing.expect(b.constCount() >= 3);

    var i: usize = 0;
    while (i < b.constCount()) : (i += 1) {
        _ = b.constKind(i);
        _ = b.constBitsAt(i);
    }

    _ = b.instCount();
    _ = b.blockCount();
    _ = b.inTerminatedBlock();
}
