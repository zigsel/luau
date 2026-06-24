//! Idiomatic Zig wrapper over the FULL Luau CodeGen x64 assembler
//! (`Luau::CodeGen::X64::AssemblyBuilderX64`, via the `asm_x64` shim).
//!
//! Every public instruction and method is bound. Operands (`Reg`, `Operand`)
//! and `Label` are heap-owned handles released with their `deinit`; the
//! `Assembler` owns the builder.
//!
//! Build code by hand, `finalize()`, then read back `code()` / `data()` and the
//! optional `text()` disassembly.

const std = @import("std");
const c = @import("bindings");

/// Operand size, mirrors `SizeX64`.
pub const Size = enum(c_uint) {
    none = c.LUAU_X64_SIZE_NONE,
    byte = c.LUAU_X64_SIZE_BYTE,
    word = c.LUAU_X64_SIZE_WORD,
    dword = c.LUAU_X64_SIZE_DWORD,
    qword = c.LUAU_X64_SIZE_QWORD,
    xmmword = c.LUAU_X64_SIZE_XMMWORD,
    ymmword = c.LUAU_X64_SIZE_YMMWORD,
};

/// Condition code, mirrors `ConditionX64`.
pub const Condition = enum(c_uint) {
    overflow = c.LUAU_X64_COND_OVERFLOW,
    no_overflow = c.LUAU_X64_COND_NO_OVERFLOW,
    carry = c.LUAU_X64_COND_CARRY,
    no_carry = c.LUAU_X64_COND_NO_CARRY,
    below = c.LUAU_X64_COND_BELOW,
    below_equal = c.LUAU_X64_COND_BELOW_EQUAL,
    above = c.LUAU_X64_COND_ABOVE,
    above_equal = c.LUAU_X64_COND_ABOVE_EQUAL,
    equal = c.LUAU_X64_COND_EQUAL,
    less = c.LUAU_X64_COND_LESS,
    less_equal = c.LUAU_X64_COND_LESS_EQUAL,
    greater = c.LUAU_X64_COND_GREATER,
    greater_equal = c.LUAU_X64_COND_GREATER_EQUAL,
    not_below = c.LUAU_X64_COND_NOT_BELOW,
    not_below_equal = c.LUAU_X64_COND_NOT_BELOW_EQUAL,
    not_above = c.LUAU_X64_COND_NOT_ABOVE,
    not_above_equal = c.LUAU_X64_COND_NOT_ABOVE_EQUAL,
    not_equal = c.LUAU_X64_COND_NOT_EQUAL,
    not_less = c.LUAU_X64_COND_NOT_LESS,
    not_less_equal = c.LUAU_X64_COND_NOT_LESS_EQUAL,
    not_greater = c.LUAU_X64_COND_NOT_GREATER,
    not_greater_equal = c.LUAU_X64_COND_NOT_GREATER_EQUAL,
    zero = c.LUAU_X64_COND_ZERO,
    not_zero = c.LUAU_X64_COND_NOT_ZERO,
    parity = c.LUAU_X64_COND_PARITY,
    not_parity = c.LUAU_X64_COND_NOT_PARITY,
};

/// Calling convention, mirrors `ABIX64`.
pub const Abi = enum(c_uint) {
    windows = c.LUAU_X64_ABI_WINDOWS,
    systemv = c.LUAU_X64_ABI_SYSTEMV,
};

/// SSE rounding mode, mirrors `RoundingModeX64`.
pub const RoundingMode = enum(c_uint) {
    nearest_even = c.LUAU_X64_ROUND_NEAREST_EVEN,
    neg_inf = c.LUAU_X64_ROUND_NEG_INF,
    pos_inf = c.LUAU_X64_ROUND_POS_INF,
    to_zero = c.LUAU_X64_ROUND_ZERO,
};

/// Fill byte for `align`, mirrors `AlignmentDataX64`.
pub const AlignmentData = enum(c_uint) {
    nop = c.LUAU_X64_ALIGN_NOP,
    int3 = c.LUAU_X64_ALIGN_INT3,
    ud2 = c.LUAU_X64_ALIGN_UD2,
};

/// CPU feature bits, mirrors `FeaturesX64`.
pub const Features = struct {
    pub const fma3: c_uint = c.LUAU_X64_FEATURE_FMA3;
    pub const avx: c_uint = c.LUAU_X64_FEATURE_AVX;
};

/// A general / vector register (`RegisterX64`), owning a heap copy.
pub const Reg = struct {
    handle: *c.LuauX64Reg,

    /// Construct from explicit size + index.
    pub fn init(reg_size: Size, reg_index: u8) Reg {
        return .{ .handle = c.luau_x64_reg(@intFromEnum(reg_size), reg_index).? };
    }
    pub fn deinit(self: Reg) void {
        c.luau_x64_reg_free(self.handle);
    }
    pub fn clone(self: Reg) Reg {
        return .{ .handle = c.luau_x64_reg_clone(self.handle).? };
    }
    pub fn size(self: Reg) Size {
        return @enumFromInt(c.luau_x64_reg_size(self.handle));
    }
    pub fn index(self: Reg) u8 {
        return c.luau_x64_reg_index(self.handle);
    }
    pub fn asByte(self: Reg) Reg {
        return .{ .handle = c.luau_x64_reg_byte(self.handle).? };
    }
    pub fn asWord(self: Reg) Reg {
        return .{ .handle = c.luau_x64_reg_word(self.handle).? };
    }
    pub fn asDword(self: Reg) Reg {
        return .{ .handle = c.luau_x64_reg_dword(self.handle).? };
    }
    pub fn asQword(self: Reg) Reg {
        return .{ .handle = c.luau_x64_reg_qword(self.handle).? };
    }

    /// Wrap this register as a register operand.
    pub fn op(self: Reg) Operand {
        return .{ .handle = c.luau_x64_op_reg(self.handle).? };
    }
};

/// Named-register factories. Each returns a fresh handle — `deinit` it.
pub const reg = struct {
    fn make(f: anytype) Reg {
        return .{ .handle = f().? };
    }
    pub fn noreg() Reg {
        return make(c.luau_x64_reg_noreg);
    }
    pub fn rip() Reg {
        return make(c.luau_x64_reg_rip);
    }

    pub fn al() Reg {
        return make(c.luau_x64_reg_al);
    }
    pub fn cl() Reg {
        return make(c.luau_x64_reg_cl);
    }
    pub fn dl() Reg {
        return make(c.luau_x64_reg_dl);
    }
    pub fn bl() Reg {
        return make(c.luau_x64_reg_bl);
    }
    pub fn spl() Reg {
        return make(c.luau_x64_reg_spl);
    }
    pub fn bpl() Reg {
        return make(c.luau_x64_reg_bpl);
    }
    pub fn sil() Reg {
        return make(c.luau_x64_reg_sil);
    }
    pub fn dil() Reg {
        return make(c.luau_x64_reg_dil);
    }
    pub fn r8b() Reg {
        return make(c.luau_x64_reg_r8b);
    }
    pub fn r9b() Reg {
        return make(c.luau_x64_reg_r9b);
    }
    pub fn r10b() Reg {
        return make(c.luau_x64_reg_r10b);
    }
    pub fn r11b() Reg {
        return make(c.luau_x64_reg_r11b);
    }
    pub fn r12b() Reg {
        return make(c.luau_x64_reg_r12b);
    }
    pub fn r13b() Reg {
        return make(c.luau_x64_reg_r13b);
    }
    pub fn r14b() Reg {
        return make(c.luau_x64_reg_r14b);
    }
    pub fn r15b() Reg {
        return make(c.luau_x64_reg_r15b);
    }

    pub fn eax() Reg {
        return make(c.luau_x64_reg_eax);
    }
    pub fn ecx() Reg {
        return make(c.luau_x64_reg_ecx);
    }
    pub fn edx() Reg {
        return make(c.luau_x64_reg_edx);
    }
    pub fn ebx() Reg {
        return make(c.luau_x64_reg_ebx);
    }
    pub fn esp() Reg {
        return make(c.luau_x64_reg_esp);
    }
    pub fn ebp() Reg {
        return make(c.luau_x64_reg_ebp);
    }
    pub fn esi() Reg {
        return make(c.luau_x64_reg_esi);
    }
    pub fn edi() Reg {
        return make(c.luau_x64_reg_edi);
    }
    pub fn r8d() Reg {
        return make(c.luau_x64_reg_r8d);
    }
    pub fn r9d() Reg {
        return make(c.luau_x64_reg_r9d);
    }
    pub fn r10d() Reg {
        return make(c.luau_x64_reg_r10d);
    }
    pub fn r11d() Reg {
        return make(c.luau_x64_reg_r11d);
    }
    pub fn r12d() Reg {
        return make(c.luau_x64_reg_r12d);
    }
    pub fn r13d() Reg {
        return make(c.luau_x64_reg_r13d);
    }
    pub fn r14d() Reg {
        return make(c.luau_x64_reg_r14d);
    }
    pub fn r15d() Reg {
        return make(c.luau_x64_reg_r15d);
    }

    pub fn rax() Reg {
        return make(c.luau_x64_reg_rax);
    }
    pub fn rcx() Reg {
        return make(c.luau_x64_reg_rcx);
    }
    pub fn rdx() Reg {
        return make(c.luau_x64_reg_rdx);
    }
    pub fn rbx() Reg {
        return make(c.luau_x64_reg_rbx);
    }
    pub fn rsp() Reg {
        return make(c.luau_x64_reg_rsp);
    }
    pub fn rbp() Reg {
        return make(c.luau_x64_reg_rbp);
    }
    pub fn rsi() Reg {
        return make(c.luau_x64_reg_rsi);
    }
    pub fn rdi() Reg {
        return make(c.luau_x64_reg_rdi);
    }
    pub fn r8() Reg {
        return make(c.luau_x64_reg_r8);
    }
    pub fn r9() Reg {
        return make(c.luau_x64_reg_r9);
    }
    pub fn r10() Reg {
        return make(c.luau_x64_reg_r10);
    }
    pub fn r11() Reg {
        return make(c.luau_x64_reg_r11);
    }
    pub fn r12() Reg {
        return make(c.luau_x64_reg_r12);
    }
    pub fn r13() Reg {
        return make(c.luau_x64_reg_r13);
    }
    pub fn r14() Reg {
        return make(c.luau_x64_reg_r14);
    }
    pub fn r15() Reg {
        return make(c.luau_x64_reg_r15);
    }

    pub fn xmm0() Reg {
        return make(c.luau_x64_reg_xmm0);
    }
    pub fn xmm1() Reg {
        return make(c.luau_x64_reg_xmm1);
    }
    pub fn xmm2() Reg {
        return make(c.luau_x64_reg_xmm2);
    }
    pub fn xmm3() Reg {
        return make(c.luau_x64_reg_xmm3);
    }
    pub fn xmm4() Reg {
        return make(c.luau_x64_reg_xmm4);
    }
    pub fn xmm5() Reg {
        return make(c.luau_x64_reg_xmm5);
    }
    pub fn xmm6() Reg {
        return make(c.luau_x64_reg_xmm6);
    }
    pub fn xmm7() Reg {
        return make(c.luau_x64_reg_xmm7);
    }
    pub fn xmm8() Reg {
        return make(c.luau_x64_reg_xmm8);
    }
    pub fn xmm9() Reg {
        return make(c.luau_x64_reg_xmm9);
    }
    pub fn xmm10() Reg {
        return make(c.luau_x64_reg_xmm10);
    }
    pub fn xmm11() Reg {
        return make(c.luau_x64_reg_xmm11);
    }
    pub fn xmm12() Reg {
        return make(c.luau_x64_reg_xmm12);
    }
    pub fn xmm13() Reg {
        return make(c.luau_x64_reg_xmm13);
    }
    pub fn xmm14() Reg {
        return make(c.luau_x64_reg_xmm14);
    }
    pub fn xmm15() Reg {
        return make(c.luau_x64_reg_xmm15);
    }

    pub fn ymm0() Reg {
        return make(c.luau_x64_reg_ymm0);
    }
    pub fn ymm1() Reg {
        return make(c.luau_x64_reg_ymm1);
    }
    pub fn ymm2() Reg {
        return make(c.luau_x64_reg_ymm2);
    }
    pub fn ymm3() Reg {
        return make(c.luau_x64_reg_ymm3);
    }
    pub fn ymm4() Reg {
        return make(c.luau_x64_reg_ymm4);
    }
    pub fn ymm5() Reg {
        return make(c.luau_x64_reg_ymm5);
    }
    pub fn ymm6() Reg {
        return make(c.luau_x64_reg_ymm6);
    }
    pub fn ymm7() Reg {
        return make(c.luau_x64_reg_ymm7);
    }
    pub fn ymm8() Reg {
        return make(c.luau_x64_reg_ymm8);
    }
    pub fn ymm9() Reg {
        return make(c.luau_x64_reg_ymm9);
    }
    pub fn ymm10() Reg {
        return make(c.luau_x64_reg_ymm10);
    }
    pub fn ymm11() Reg {
        return make(c.luau_x64_reg_ymm11);
    }
    pub fn ymm12() Reg {
        return make(c.luau_x64_reg_ymm12);
    }
    pub fn ymm13() Reg {
        return make(c.luau_x64_reg_ymm13);
    }
    pub fn ymm14() Reg {
        return make(c.luau_x64_reg_ymm14);
    }
    pub fn ymm15() Reg {
        return make(c.luau_x64_reg_ymm15);
    }
};

/// An instruction operand (`OperandX64`), owning a heap copy.
pub const Operand = struct {
    handle: *c.LuauX64Operand,

    /// Register operand.
    pub fn fromReg(r: Reg) Operand {
        return .{ .handle = c.luau_x64_op_reg(r.handle).? };
    }
    /// 32-bit immediate operand.
    pub fn imm(value: i32) Operand {
        return .{ .handle = c.luau_x64_imm(value).? };
    }
    /// Memory operand: [base + idx*scale + disp] with access `size`.
    /// `base` / `idx` may be null.
    pub fn mem(size: Size, idx: ?Reg, scale: u8, base: ?Reg, disp: i32) Operand {
        const ih: ?*c.LuauX64Reg = if (idx) |x| x.handle else null;
        const bh: ?*c.LuauX64Reg = if (base) |x| x.handle else null;
        return .{ .handle = c.luau_x64_op_mem(@intFromEnum(size), ih, scale, bh, disp).? };
    }
    pub fn clone(self: Operand) Operand {
        return .{ .handle = c.luau_x64_op_clone(self.handle).? };
    }
    pub fn deinit(self: Operand) void {
        c.luau_x64_op_free(self.handle);
    }
};

/// A jump target (`Label`), owning a heap copy.
pub const Label = struct {
    handle: *c.LuauX64Label,

    pub fn init() Label {
        return .{ .handle = c.luau_x64_label_new().? };
    }
    pub fn deinit(self: Label) void {
        c.luau_x64_label_free(self.handle);
    }
    pub fn id(self: Label) u32 {
        return c.luau_x64_label_id(self.handle);
    }
    pub fn location(self: Label) u32 {
        return c.luau_x64_label_location(self.handle);
    }
};

/// The x64 assembler builder.
pub const Assembler = struct {
    handle: *c.LuauX64Asm,

    pub const Error = error{AssemblerCreateFailed};

    /// Create with an explicit ABI.
    pub fn initAbi(log_text: bool, abi: Abi, features: c_uint) Error!Assembler {
        const h = c.luau_x64_asm_new_abi(@intFromBool(log_text), @intFromEnum(abi), features) orelse
            return Error.AssemblerCreateFailed;
        return .{ .handle = h };
    }
    /// Create with the default ABI for the platform.
    pub fn init(log_text: bool, features: c_uint) Error!Assembler {
        const h = c.luau_x64_asm_new(@intFromBool(log_text), features) orelse
            return Error.AssemblerCreateFailed;
        return .{ .handle = h };
    }
    pub fn deinit(self: Assembler) void {
        c.luau_x64_asm_free(self.handle);
    }

    // ---- base two-operand ----
    pub fn add(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_add(self.handle, lhs.handle, rhs.handle);
    }
    pub fn sub(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_sub(self.handle, lhs.handle, rhs.handle);
    }
    pub fn cmp(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_cmp(self.handle, lhs.handle, rhs.handle);
    }
    pub fn andOp(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_and(self.handle, lhs.handle, rhs.handle);
    }
    pub fn orOp(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_or(self.handle, lhs.handle, rhs.handle);
    }
    pub fn xorOp(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_xor(self.handle, lhs.handle, rhs.handle);
    }

    // ---- shifts ----
    pub fn sal(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_sal(self.handle, lhs.handle, rhs.handle);
    }
    pub fn sar(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_sar(self.handle, lhs.handle, rhs.handle);
    }
    pub fn shl(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_shl(self.handle, lhs.handle, rhs.handle);
    }
    pub fn shr(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_shr(self.handle, lhs.handle, rhs.handle);
    }
    pub fn rol(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_rol(self.handle, lhs.handle, rhs.handle);
    }
    pub fn ror(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_ror(self.handle, lhs.handle, rhs.handle);
    }

    // ---- mov family ----
    pub fn mov(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_mov(self.handle, lhs.handle, rhs.handle);
    }
    pub fn mov64(self: Assembler, lhs: Reg, value: i64) void {
        c.luau_x64_asm_mov64(self.handle, lhs.handle, value);
    }
    pub fn movsx(self: Assembler, lhs: Reg, rhs: Operand) void {
        c.luau_x64_asm_movsx(self.handle, lhs.handle, rhs.handle);
    }
    pub fn movzx(self: Assembler, lhs: Reg, rhs: Operand) void {
        c.luau_x64_asm_movzx(self.handle, lhs.handle, rhs.handle);
    }

    // ---- one-operand ----
    pub fn div(self: Assembler, op: Operand) void {
        c.luau_x64_asm_div(self.handle, op.handle);
    }
    pub fn idiv(self: Assembler, op: Operand) void {
        c.luau_x64_asm_idiv(self.handle, op.handle);
    }
    pub fn mul(self: Assembler, op: Operand) void {
        c.luau_x64_asm_mul(self.handle, op.handle);
    }
    pub fn imul1(self: Assembler, op: Operand) void {
        c.luau_x64_asm_imul1(self.handle, op.handle);
    }
    pub fn neg(self: Assembler, op: Operand) void {
        c.luau_x64_asm_neg(self.handle, op.handle);
    }
    pub fn notOp(self: Assembler, op: Operand) void {
        c.luau_x64_asm_not(self.handle, op.handle);
    }
    pub fn dec(self: Assembler, op: Operand) void {
        c.luau_x64_asm_dec(self.handle, op.handle);
    }
    pub fn inc(self: Assembler, op: Operand) void {
        c.luau_x64_asm_inc(self.handle, op.handle);
    }
    pub fn push(self: Assembler, op: Operand) void {
        c.luau_x64_asm_push(self.handle, op.handle);
    }
    pub fn pop(self: Assembler, op: Operand) void {
        c.luau_x64_asm_pop(self.handle, op.handle);
    }

    // ---- imul extra forms / misc two-operand ----
    pub fn imul2(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_imul2(self.handle, lhs.handle, rhs.handle);
    }
    pub fn imul3(self: Assembler, dst: Operand, lhs: Operand, rhs: i32) void {
        c.luau_x64_asm_imul3(self.handle, dst.handle, lhs.handle, rhs);
    }
    pub fn testOp(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_test(self.handle, lhs.handle, rhs.handle);
    }
    pub fn lea(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_lea(self.handle, lhs.handle, rhs.handle);
    }
    pub fn setcc(self: Assembler, cond: Condition, op: Operand) void {
        c.luau_x64_asm_setcc(self.handle, @intFromEnum(cond), op.handle);
    }
    pub fn cmov(self: Assembler, cond: Condition, lhs: Reg, rhs: Operand) void {
        c.luau_x64_asm_cmov(self.handle, @intFromEnum(cond), lhs.handle, rhs.handle);
    }
    pub fn ret(self: Assembler) void {
        c.luau_x64_asm_ret(self.handle);
    }

    // ---- control flow ----
    pub fn jcc(self: Assembler, cond: Condition, label: Label) void {
        c.luau_x64_asm_jcc(self.handle, @intFromEnum(cond), label.handle);
    }
    pub fn jmp(self: Assembler, label: Label) void {
        c.luau_x64_asm_jmp_label(self.handle, label.handle);
    }
    pub fn jmpOp(self: Assembler, op: Operand) void {
        c.luau_x64_asm_jmp_op(self.handle, op.handle);
    }
    pub fn call(self: Assembler, label: Label) void {
        c.luau_x64_asm_call_label(self.handle, label.handle);
    }
    pub fn callOp(self: Assembler, op: Operand) void {
        c.luau_x64_asm_call_op(self.handle, op.handle);
    }
    pub fn leaLabel(self: Assembler, lhs: Reg, label: Label) void {
        c.luau_x64_asm_lea_label(self.handle, lhs.handle, label.handle);
    }
    pub fn int3(self: Assembler) void {
        c.luau_x64_asm_int3(self.handle);
    }
    pub fn ud2(self: Assembler) void {
        c.luau_x64_asm_ud2(self.handle);
    }
    pub fn cqo(self: Assembler) void {
        c.luau_x64_asm_cqo(self.handle);
    }
    pub fn cdq(self: Assembler) void {
        c.luau_x64_asm_cdq(self.handle);
    }
    pub fn bsr(self: Assembler, dst: Reg, src: Operand) void {
        c.luau_x64_asm_bsr(self.handle, dst.handle, src.handle);
    }
    pub fn bsf(self: Assembler, dst: Reg, src: Operand) void {
        c.luau_x64_asm_bsf(self.handle, dst.handle, src.handle);
    }
    pub fn bswap(self: Assembler, dst: Reg) void {
        c.luau_x64_asm_bswap(self.handle, dst.handle);
    }

    // ---- alignment ----
    pub fn nop(self: Assembler, length: u32) void {
        c.luau_x64_asm_nop(self.handle, length);
    }
    pub fn alignTo(self: Assembler, alignment: u32, align_data: AlignmentData) void {
        c.luau_x64_asm_align(self.handle, alignment, @intFromEnum(align_data));
    }

    // ---- AVX (three-operand) ----
    pub fn vaddpd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vaddpd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vaddps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vaddps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vaddsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vaddsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vaddss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vaddss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vsubsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vsubsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vsubss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vsubss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vsubps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vsubps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmulsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmulsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmulss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmulss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmulps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmulps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vdivsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vdivsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vdivss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vdivss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vdivps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vdivps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vandps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vandps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vandpd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vandpd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vandnpd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vandnpd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vxorps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vxorps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vxorpd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vxorpd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vorps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vorps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vorpd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vorpd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vucomisd(self: Assembler, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vucomisd(self.handle, src1.handle, src2.handle);
    }
    pub fn vucomiss(self: Assembler, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vucomiss(self.handle, src1.handle, src2.handle);
    }
    pub fn vcvttsd2si(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vcvttsd2si(self.handle, dst.handle, src.handle);
    }
    pub fn vcvtsi2sd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcvtsi2sd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcvtsi2ss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcvtsi2ss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcvtsd2ss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcvtsd2ss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcvtss2sd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcvtss2sd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vroundsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand, mode: RoundingMode) void {
        c.luau_x64_asm_vroundsd(self.handle, dst.handle, src1.handle, src2.handle, @intFromEnum(mode));
    }
    pub fn vroundss(self: Assembler, dst: Operand, src1: Operand, src2: Operand, mode: RoundingMode) void {
        c.luau_x64_asm_vroundss(self.handle, dst.handle, src1.handle, src2.handle, @intFromEnum(mode));
    }
    pub fn vroundps(self: Assembler, dst: Operand, src: Operand, mode: RoundingMode) void {
        c.luau_x64_asm_vroundps(self.handle, dst.handle, src.handle, @intFromEnum(mode));
    }
    pub fn vsqrtpd(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vsqrtpd(self.handle, dst.handle, src.handle);
    }
    pub fn vsqrtps(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vsqrtps(self.handle, dst.handle, src.handle);
    }
    pub fn vsqrtsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vsqrtsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vsqrtss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vsqrtss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmovsd2(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovsd2(self.handle, dst.handle, src.handle);
    }
    pub fn vmovsd3(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmovsd3(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmovss2(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovss2(self.handle, dst.handle, src.handle);
    }
    pub fn vmovss3(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmovss3(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmovapd(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovapd(self.handle, dst.handle, src.handle);
    }
    pub fn vmovaps(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovaps(self.handle, dst.handle, src.handle);
    }
    pub fn vmovupd(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovupd(self.handle, dst.handle, src.handle);
    }
    pub fn vmovups(self: Assembler, dst: Operand, src: Operand) void {
        c.luau_x64_asm_vmovups(self.handle, dst.handle, src.handle);
    }
    pub fn vmovq(self: Assembler, lhs: Operand, rhs: Operand) void {
        c.luau_x64_asm_vmovq(self.handle, lhs.handle, rhs.handle);
    }
    pub fn vmaxps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmaxps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmaxsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmaxsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vmaxss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vmaxss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vminps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vminps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vminsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vminsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vminss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vminss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcmpeqsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcmpeqsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcmpltsd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcmpltsd(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcmpltss(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcmpltss(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vcmpeqps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vcmpeqps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vblendvps(self: Assembler, dst: Reg, src1: Reg, src2: Operand, mask: Reg) void {
        c.luau_x64_asm_vblendvps(self.handle, dst.handle, src1.handle, src2.handle, mask.handle);
    }
    pub fn vblendvpd(self: Assembler, dst: Reg, src1: Reg, src2: Operand, mask: Reg) void {
        c.luau_x64_asm_vblendvpd(self.handle, dst.handle, src1.handle, src2.handle, mask.handle);
    }
    pub fn vpshufps(self: Assembler, dst: Reg, src1: Reg, src2: Operand, shuffle: u8) void {
        c.luau_x64_asm_vpshufps(self.handle, dst.handle, src1.handle, src2.handle, shuffle);
    }
    pub fn vpinsrd(self: Assembler, dst: Reg, src1: Reg, src2: Operand, offset: u8) void {
        c.luau_x64_asm_vpinsrd(self.handle, dst.handle, src1.handle, src2.handle, offset);
    }
    pub fn vpextrd(self: Assembler, dst: Reg, src: Reg, offset: u8) void {
        c.luau_x64_asm_vpextrd(self.handle, dst.handle, src.handle, offset);
    }
    pub fn vdpps(self: Assembler, dst: Operand, src1: Operand, src2: Operand, mask: u8) void {
        c.luau_x64_asm_vdpps(self.handle, dst.handle, src1.handle, src2.handle, mask);
    }
    pub fn vfmadd213ps(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vfmadd213ps(self.handle, dst.handle, src1.handle, src2.handle);
    }
    pub fn vfmadd213pd(self: Assembler, dst: Operand, src1: Operand, src2: Operand) void {
        c.luau_x64_asm_vfmadd213pd(self.handle, dst.handle, src1.handle, src2.handle);
    }

    // ---- labels ----
    /// Place a fresh label here and write it into `out`.
    pub fn setLabelHere(self: Assembler, out: Label) void {
        c.luau_x64_asm_set_label_here(self.handle, out.handle);
    }
    /// Bind an existing label to the current location.
    pub fn placeLabel(self: Assembler, label: Label) void {
        c.luau_x64_asm_place_label(self.handle, label.handle);
    }
    pub fn labelOffset(self: Assembler, label: Label) u32 {
        return c.luau_x64_asm_label_offset(self.handle, label.handle);
    }

    // ---- constant allocation (rip-relative) ----
    pub fn i32Const(self: Assembler, value: i32) Operand {
        return .{ .handle = c.luau_x64_asm_i32(self.handle, value).? };
    }
    pub fn i64Const(self: Assembler, value: i64) Operand {
        return .{ .handle = c.luau_x64_asm_i64(self.handle, value).? };
    }
    pub fn f32Const(self: Assembler, value: f32) Operand {
        return .{ .handle = c.luau_x64_asm_f32(self.handle, value).? };
    }
    pub fn f64Const(self: Assembler, value: f64) Operand {
        return .{ .handle = c.luau_x64_asm_f64(self.handle, value).? };
    }
    pub fn u32x4(self: Assembler, x: u32, y: u32, z: u32, w: u32) Operand {
        return .{ .handle = c.luau_x64_asm_u32x4(self.handle, x, y, z, w).? };
    }
    pub fn f32x4(self: Assembler, x: f32, y: f32, z: f32, w: f32) Operand {
        return .{ .handle = c.luau_x64_asm_f32x4(self.handle, x, y, z, w).? };
    }
    pub fn f64x2(self: Assembler, x: f64, y: f64) Operand {
        return .{ .handle = c.luau_x64_asm_f64x2(self.handle, x, y).? };
    }
    pub fn bytes(self: Assembler, blob: []const u8, alignment: usize) Operand {
        return .{ .handle = c.luau_x64_asm_bytes(self.handle, blob.ptr, blob.len, alignment).? };
    }

    // ---- finalize / output ----
    pub fn finalize(self: Assembler) bool {
        return c.luau_x64_asm_finalize(self.handle) != 0;
    }
    pub fn codeSize(self: Assembler) u32 {
        return c.luau_x64_asm_code_size(self.handle);
    }
    pub fn instructionCount(self: Assembler) c_uint {
        return c.luau_x64_asm_instruction_count(self.handle);
    }
    /// Emitted machine code bytes (valid until the builder is freed/mutated).
    pub fn code(self: Assembler) []const u8 {
        var len: usize = 0;
        const ptr = c.luau_x64_asm_code_ptr(self.handle, &len);
        if (ptr == null or len == 0) return &.{};
        return ptr[0..len];
    }
    /// The rip-relative constant data blob.
    pub fn data(self: Assembler) []const u8 {
        var len: usize = 0;
        const ptr = c.luau_x64_asm_data_ptr(self.handle, &len);
        if (ptr == null or len == 0) return &.{};
        return ptr[0..len];
    }
    /// Textual disassembly (caller frees with `allocator` matching `free`).
    /// Returns null when logging is off. Free the returned slice's pointer via
    /// `std.c.free`.
    pub fn text(self: Assembler) ?[:0]u8 {
        const ptr = c.luau_x64_asm_text(self.handle) orelse return null;
        return std.mem.span(ptr);
    }
    pub fn logAppend(self: Assembler, str: [*:0]const u8) void {
        c.luau_x64_asm_log_append(self.handle, str);
    }
};
