//! Idiomatic wrapper over the Luau CodeGen AArch64 assembler
//! (`Luau::CodeGen::A64::AssemblyBuilderA64`, via the C++ shim) — the low-level
//! arm64 JIT construction kit. Build machine code by hand, then read back the
//! emitted instruction words.
//!
//! Operands are value types in C++ (`RegisterA64`, `AddressA64`); here they are
//! owned opaque handles (`Reg`, `Address`) you build with the factory functions
//! and pass to emit methods. Free them with `deinit`. See
//! `shim/codegen/asm_a64.h`.

const std = @import("std");
const c = @import("bindings");

/// Register kind, mirrors `KindA64`.
pub const Kind = enum(c_uint) {
    none = c.LUAU_A64_KIND_NONE,
    w = c.LUAU_A64_KIND_W, // 32-bit GPR
    x = c.LUAU_A64_KIND_X, // 64-bit GPR
    s = c.LUAU_A64_KIND_S, // 32-bit SIMD&FP scalar
    d = c.LUAU_A64_KIND_D, // 64-bit SIMD&FP scalar
    q = c.LUAU_A64_KIND_Q, // 128-bit SIMD&FP vector
};

/// Condition code, mirrors `ConditionA64`.
pub const Condition = enum(c_uint) {
    equal = c.LUAU_A64_COND_EQUAL,
    not_equal = c.LUAU_A64_COND_NOT_EQUAL,
    carry_set = c.LUAU_A64_COND_CARRY_SET,
    carry_clear = c.LUAU_A64_COND_CARRY_CLEAR,
    minus = c.LUAU_A64_COND_MINUS,
    plus = c.LUAU_A64_COND_PLUS,
    overflow = c.LUAU_A64_COND_OVERFLOW,
    no_overflow = c.LUAU_A64_COND_NO_OVERFLOW,
    unsigned_greater = c.LUAU_A64_COND_UNSIGNED_GREATER,
    unsigned_less_equal = c.LUAU_A64_COND_UNSIGNED_LESS_EQUAL,
    greater_equal = c.LUAU_A64_COND_GREATER_EQUAL,
    less = c.LUAU_A64_COND_LESS,
    greater = c.LUAU_A64_COND_GREATER,
    less_equal = c.LUAU_A64_COND_LESS_EQUAL,
    always = c.LUAU_A64_COND_ALWAYS,
};

/// Address addressing mode, mirrors `AddressKindA64`.
pub const AddressKind = enum(c_uint) {
    reg = c.LUAU_A64_ADDR_REG, // reg + reg
    imm = c.LUAU_A64_ADDR_IMM, // reg + imm
    pre = c.LUAU_A64_ADDR_PRE, // reg + imm, reg += imm
    post = c.LUAU_A64_ADDR_POST, // reg, reg += imm
};

/// Hardware feature bits, mirrors `FeaturesA64`.
pub const Feature = struct {
    pub const jscvt: c_uint = c.LUAU_A64_FEATURE_JSCVT;
    pub const advsimd: c_uint = c.LUAU_A64_FEATURE_ADVSIMD;
};

pub const Error = error{ OutOfMemory, FinalizeFailed };

/// An owned register operand handle (a heap copy of a `RegisterA64`).
pub const Reg = struct {
    ptr: *c.LuauA64Reg,

    /// Build a register from (kind, index). index in 0..31.
    pub fn init(reg_kind: Kind, reg_index: u8) Error!Reg {
        const p = c.luau_a64_reg(@intFromEnum(reg_kind), reg_index) orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    /// The stack pointer (sp).
    pub fn sp() Error!Reg {
        const p = c.luau_a64_reg_sp() orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    /// The "no register" sentinel.
    pub fn noreg() Error!Reg {
        const p = c.luau_a64_reg_noreg() orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    /// Reinterpret this register as another kind (same index).
    pub fn cast(self: Reg, to_kind: Kind) Error!Reg {
        const p = c.luau_a64_reg_cast(@intFromEnum(to_kind), self.ptr) orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    pub fn deinit(self: Reg) void {
        c.luau_a64_reg_free(self.ptr);
    }

    pub fn kind(self: Reg) Kind {
        return @enumFromInt(c.luau_a64_reg_kind(self.ptr));
    }

    pub fn index(self: Reg) u8 {
        return @intCast(c.luau_a64_reg_index(self.ptr));
    }
};

/// An owned address operand handle (a heap copy of an `AddressA64`).
pub const Address = struct {
    ptr: *c.LuauA64Address,

    /// base + imm (kind imm/pre/post). base must be an X register or sp.
    pub fn initImm(base: Reg, offset: i32, kind: AddressKind) Error!Address {
        const p = c.luau_a64_address_imm(base.ptr, offset, @intFromEnum(kind)) orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    /// base + offset register. Both must be X registers.
    pub fn initReg(base: Reg, offset: Reg) Error!Address {
        const p = c.luau_a64_address_reg(base.ptr, offset.ptr) orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    pub fn deinit(self: Address) void {
        c.luau_a64_address_free(self.ptr);
    }
};

/// A label, referenced by id.
pub const Label = u32;

/// The AArch64 assembly builder.
pub const Builder = struct {
    ptr: *c.LuauAsmA64,

    pub fn init(log_text: bool, features: c_uint) Error!Builder {
        const p = c.luau_a64_asm_new(@intFromBool(log_text), features) orelse return error.OutOfMemory;
        return .{ .ptr = p };
    }

    pub fn deinit(self: Builder) void {
        c.luau_a64_asm_free(self.ptr);
    }

    // ---- labels ----------------------------------------------------------

    pub fn setLabelHere(self: Builder) Label {
        return c.luau_a64_asm_set_label_here(self.ptr);
    }
    pub fn makeLabel(self: Builder) Label {
        return c.luau_a64_asm_make_label(self.ptr);
    }
    pub fn placeLabel(self: Builder, label: Label) void {
        c.luau_a64_asm_place_label(self.ptr, label);
    }
    pub fn labelOffset(self: Builder, label: Label) u32 {
        return c.luau_a64_asm_label_offset(self.ptr, label);
    }

    // ---- finalize / output ----------------------------------------------

    pub fn finalize(self: Builder) Error!void {
        if (c.luau_a64_asm_finalize(self.ptr) == 0) return error.FinalizeFailed;
    }
    pub fn codeSize(self: Builder) u32 {
        return c.luau_a64_asm_code_size(self.ptr);
    }
    pub fn instructionCount(self: Builder) u32 {
        return c.luau_a64_asm_instruction_count(self.ptr);
    }
    /// Emitted machine code as bytes (length is a multiple of 4).
    pub fn code(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_a64_asm_code_ptr(self.ptr, &len);
        if (p == null or len == 0) return &[_]u8{};
        return p[0..len];
    }
    /// Number of emitted 32-bit instruction words.
    pub fn codeWordCount(self: Builder) usize {
        return c.luau_a64_asm_code_word_count(self.ptr);
    }
    /// Emitted machine code as 32-bit instruction words.
    pub fn codeWords(self: Builder) []const u32 {
        const bytes = self.code();
        return std.mem.bytesAsSlice(u32, bytes);
    }
    /// rip-relative constant data blob.
    pub fn data(self: Builder) []const u8 {
        var len: usize = 0;
        const p = c.luau_a64_asm_data_ptr(self.ptr, &len);
        if (p == null or len == 0) return &[_]u8{};
        return p[0..len];
    }
    /// Textual disassembly (only when log_text). Caller owns; free with allocator? No —
    /// the shim malloc's it; free with std.c.free.
    pub fn text(self: Builder) ?[*:0]u8 {
        return c.luau_a64_asm_get_text(self.ptr);
    }
    pub fn logAppend(self: Builder, s: [*:0]const u8) void {
        c.luau_a64_asm_log_append(self.ptr, s);
    }

    // ---- static helpers --------------------------------------------------

    pub fn isMaskSupported(mask: u32) bool {
        return c.luau_a64_asm_is_mask_supported(mask) != 0;
    }
    pub fn isFmovSupportedFp64(value: f64) bool {
        return c.luau_a64_asm_is_fmov_supported_fp64(value) != 0;
    }
    pub fn isFmovSupportedFp32(value: f32) bool {
        return c.luau_a64_asm_is_fmov_supported_fp32(value) != 0;
    }

    // ---- Moves -----------------------------------------------------------

    pub fn mov(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_mov(self.ptr, dst.ptr, src.ptr);
    }
    pub fn movImm(self: Builder, dst: Reg, src: i32) void {
        c.luau_a64_asm_mov_imm(self.ptr, dst.ptr, src);
    }
    pub fn movz(self: Builder, dst: Reg, src: u16, shift: i32) void {
        c.luau_a64_asm_movz(self.ptr, dst.ptr, src, shift);
    }
    pub fn movn(self: Builder, dst: Reg, src: u16, shift: i32) void {
        c.luau_a64_asm_movn(self.ptr, dst.ptr, src, shift);
    }
    pub fn movk(self: Builder, dst: Reg, src: u16, shift: i32) void {
        c.luau_a64_asm_movk(self.ptr, dst.ptr, src, shift);
    }

    // ---- Arithmetic ------------------------------------------------------

    pub fn add(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_add(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn addImm(self: Builder, dst: Reg, src1: Reg, src2: u16) void {
        c.luau_a64_asm_add_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn sub(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_sub(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn subImm(self: Builder, dst: Reg, src1: Reg, src2: u16) void {
        c.luau_a64_asm_sub_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn neg(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_neg(self.ptr, dst.ptr, src.ptr);
    }
    pub fn mul(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_mul(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn msub(self: Builder, dst: Reg, src1: Reg, src2: Reg, src3: Reg) void {
        c.luau_a64_asm_msub(self.ptr, dst.ptr, src1.ptr, src2.ptr, src3.ptr);
    }
    pub fn sdiv(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_sdiv(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn udiv(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_udiv(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn rem(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_rem(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }

    // ---- Comparisons -----------------------------------------------------

    pub fn cmp(self: Builder, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_cmp(self.ptr, src1.ptr, src2.ptr);
    }
    pub fn cmpImm(self: Builder, src1: Reg, src2: u16) void {
        c.luau_a64_asm_cmp_imm(self.ptr, src1.ptr, src2);
    }
    pub fn ccmp(self: Builder, src1: Reg, src2: Reg, cond: Condition, nzcv: u8) void {
        c.luau_a64_asm_ccmp(self.ptr, src1.ptr, src2.ptr, @intFromEnum(cond), nzcv);
    }
    pub fn ccmn(self: Builder, src1: Reg, src2: Reg, cond: Condition, nzcv: u8) void {
        c.luau_a64_asm_ccmn(self.ptr, src1.ptr, src2.ptr, @intFromEnum(cond), nzcv);
    }
    pub fn ccmnImm(self: Builder, src1: Reg, src2: u8, cond: Condition, nzcv: u8) void {
        c.luau_a64_asm_ccmn_imm(self.ptr, src1.ptr, src2, @intFromEnum(cond), nzcv);
    }
    pub fn cmnImm(self: Builder, src1: Reg, src2: u16) void {
        c.luau_a64_asm_cmn_imm(self.ptr, src1.ptr, src2);
    }
    pub fn csel(self: Builder, dst: Reg, src1: Reg, src2: Reg, cond: Condition) void {
        c.luau_a64_asm_csel(self.ptr, dst.ptr, src1.ptr, src2.ptr, @intFromEnum(cond));
    }
    pub fn cset(self: Builder, dst: Reg, cond: Condition) void {
        c.luau_a64_asm_cset(self.ptr, dst.ptr, @intFromEnum(cond));
    }

    // ---- Bitwise ---------------------------------------------------------

    pub fn andRr(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_and(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn orr(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_orr(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn eor(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_eor(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn bic(self: Builder, dst: Reg, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_bic(self.ptr, dst.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn tst(self: Builder, src1: Reg, src2: Reg, shift: i32) void {
        c.luau_a64_asm_tst(self.ptr, src1.ptr, src2.ptr, shift);
    }
    pub fn mvn(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_mvn(self.ptr, dst.ptr, src.ptr);
    }
    pub fn andImm(self: Builder, dst: Reg, src1: Reg, src2: u32) void {
        c.luau_a64_asm_and_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn orrImm(self: Builder, dst: Reg, src1: Reg, src2: u32) void {
        c.luau_a64_asm_orr_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn eorImm(self: Builder, dst: Reg, src1: Reg, src2: u32) void {
        c.luau_a64_asm_eor_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn tstImm(self: Builder, src1: Reg, src2: u32) void {
        c.luau_a64_asm_tst_imm(self.ptr, src1.ptr, src2);
    }

    // ---- Shifts ----------------------------------------------------------

    pub fn lsl(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_lsl(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn lsr(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_lsr(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn asr(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_asr(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn ror(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_ror(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn clz(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_clz(self.ptr, dst.ptr, src.ptr);
    }
    pub fn rbit(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_rbit(self.ptr, dst.ptr, src.ptr);
    }
    pub fn rev(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_rev(self.ptr, dst.ptr, src.ptr);
    }
    pub fn lslImm(self: Builder, dst: Reg, src1: Reg, src2: u8) void {
        c.luau_a64_asm_lsl_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn lsrImm(self: Builder, dst: Reg, src1: Reg, src2: u8) void {
        c.luau_a64_asm_lsr_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn asrImm(self: Builder, dst: Reg, src1: Reg, src2: u8) void {
        c.luau_a64_asm_asr_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }
    pub fn rorImm(self: Builder, dst: Reg, src1: Reg, src2: u8) void {
        c.luau_a64_asm_ror_imm(self.ptr, dst.ptr, src1.ptr, src2);
    }

    // ---- Bitfields -------------------------------------------------------

    pub fn ubfiz(self: Builder, dst: Reg, src: Reg, f: u8, w: u8) void {
        c.luau_a64_asm_ubfiz(self.ptr, dst.ptr, src.ptr, f, w);
    }
    pub fn ubfx(self: Builder, dst: Reg, src: Reg, f: u8, w: u8) void {
        c.luau_a64_asm_ubfx(self.ptr, dst.ptr, src.ptr, f, w);
    }
    pub fn sbfiz(self: Builder, dst: Reg, src: Reg, f: u8, w: u8) void {
        c.luau_a64_asm_sbfiz(self.ptr, dst.ptr, src.ptr, f, w);
    }
    pub fn sbfx(self: Builder, dst: Reg, src: Reg, f: u8, w: u8) void {
        c.luau_a64_asm_sbfx(self.ptr, dst.ptr, src.ptr, f, w);
    }

    // ---- Loads -----------------------------------------------------------

    pub fn ldr(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldr(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldrb(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldrb(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldrh(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldrh(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldrsb(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldrsb(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldrsh(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldrsh(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldrsw(self: Builder, dst: Reg, src: Address) void {
        c.luau_a64_asm_ldrsw(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ldp(self: Builder, dst1: Reg, dst2: Reg, src: Address) void {
        c.luau_a64_asm_ldp(self.ptr, dst1.ptr, dst2.ptr, src.ptr);
    }

    // ---- Stores ----------------------------------------------------------

    pub fn str(self: Builder, src: Reg, dst: Address) void {
        c.luau_a64_asm_str(self.ptr, src.ptr, dst.ptr);
    }
    pub fn strb(self: Builder, src: Reg, dst: Address) void {
        c.luau_a64_asm_strb(self.ptr, src.ptr, dst.ptr);
    }
    pub fn strh(self: Builder, src: Reg, dst: Address) void {
        c.luau_a64_asm_strh(self.ptr, src.ptr, dst.ptr);
    }
    pub fn stp(self: Builder, src1: Reg, src2: Reg, dst: Address) void {
        c.luau_a64_asm_stp(self.ptr, src1.ptr, src2.ptr, dst.ptr);
    }

    // ---- Control flow ----------------------------------------------------

    pub fn b(self: Builder, label: Label) void {
        c.luau_a64_asm_b(self.ptr, label);
    }
    pub fn bl(self: Builder, label: Label) void {
        c.luau_a64_asm_bl(self.ptr, label);
    }
    pub fn br(self: Builder, src: Reg) void {
        c.luau_a64_asm_br(self.ptr, src.ptr);
    }
    pub fn blr(self: Builder, src: Reg) void {
        c.luau_a64_asm_blr(self.ptr, src.ptr);
    }
    pub fn ret(self: Builder) void {
        c.luau_a64_asm_ret(self.ptr);
    }
    pub fn bCond(self: Builder, cond: Condition, label: Label) void {
        c.luau_a64_asm_b_cond(self.ptr, @intFromEnum(cond), label);
    }
    pub fn cbz(self: Builder, src: Reg, label: Label) void {
        c.luau_a64_asm_cbz(self.ptr, src.ptr, label);
    }
    pub fn cbnz(self: Builder, src: Reg, label: Label) void {
        c.luau_a64_asm_cbnz(self.ptr, src.ptr, label);
    }
    pub fn tbz(self: Builder, src: Reg, bit_index: u8, label: Label) void {
        c.luau_a64_asm_tbz(self.ptr, src.ptr, bit_index, label);
    }
    pub fn tbnz(self: Builder, src: Reg, bit_index: u8, label: Label) void {
        c.luau_a64_asm_tbnz(self.ptr, src.ptr, bit_index, label);
    }

    // ---- Address of embedded data / code --------------------------------

    pub fn adrData(self: Builder, dst: Reg, ptr: ?*const anyopaque, size: usize) void {
        c.luau_a64_asm_adr_data(self.ptr, dst.ptr, ptr, size);
    }
    pub fn adrU64(self: Builder, dst: Reg, value: u64) void {
        c.luau_a64_asm_adr_u64(self.ptr, dst.ptr, value);
    }
    pub fn adrF32(self: Builder, dst: Reg, value: f32) void {
        c.luau_a64_asm_adr_f32(self.ptr, dst.ptr, value);
    }
    pub fn adrF64(self: Builder, dst: Reg, value: f64) void {
        c.luau_a64_asm_adr_f64(self.ptr, dst.ptr, value);
    }
    pub fn adrLabel(self: Builder, dst: Reg, label: Label) void {
        c.luau_a64_asm_adr_label(self.ptr, dst.ptr, label);
    }

    // ---- FP / SIMD moves -------------------------------------------------

    pub fn fmov(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fmov(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fmovF64(self: Builder, dst: Reg, src: f64) void {
        c.luau_a64_asm_fmov_f64(self.ptr, dst.ptr, src);
    }
    pub fn fmovF32(self: Builder, dst: Reg, src: f32) void {
        c.luau_a64_asm_fmov_f32(self.ptr, dst.ptr, src);
    }

    // ---- FP / SIMD math --------------------------------------------------

    pub fn fabs(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fabs(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fadd(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fadd(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn fdiv(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fdiv(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn fmul(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fmul(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn fneg(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fneg(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fsqrt(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fsqrt(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fsub(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fsub(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn faddp(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_faddp(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fmla(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fmla(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }

    // ---- Vector component manipulation ----------------------------------

    pub fn ins4s(self: Builder, dst: Reg, src: Reg, idx: u8) void {
        c.luau_a64_asm_ins_4s(self.ptr, dst.ptr, src.ptr, idx);
    }
    pub fn ins4sIdx(self: Builder, dst: Reg, dst_index: u8, src: Reg, src_index: u8) void {
        c.luau_a64_asm_ins_4s_idx(self.ptr, dst.ptr, dst_index, src.ptr, src_index);
    }
    pub fn dup4s(self: Builder, dst: Reg, src: Reg, idx: u8) void {
        c.luau_a64_asm_dup_4s(self.ptr, dst.ptr, src.ptr, idx);
    }
    pub fn umov4s(self: Builder, dst: Reg, src: Reg, idx: u8) void {
        c.luau_a64_asm_umov_4s(self.ptr, dst.ptr, src.ptr, idx);
    }
    pub fn fcmeq4s(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fcmeq_4s(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn fcmgt4s(self: Builder, dst: Reg, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fcmgt_4s(self.ptr, dst.ptr, src1.ptr, src2.ptr);
    }
    pub fn bit(self: Builder, dst: Reg, src: Reg, mask: Reg) void {
        c.luau_a64_asm_bit(self.ptr, dst.ptr, src.ptr, mask.ptr);
    }
    pub fn bif(self: Builder, dst: Reg, src: Reg, mask: Reg) void {
        c.luau_a64_asm_bif(self.ptr, dst.ptr, src.ptr, mask.ptr);
    }

    // ---- FP rounding and conversions ------------------------------------

    pub fn frinta(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_frinta(self.ptr, dst.ptr, src.ptr);
    }
    pub fn frintm(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_frintm(self.ptr, dst.ptr, src.ptr);
    }
    pub fn frintp(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_frintp(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fcvt(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fcvt(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fcvtzs(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fcvtzs(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fcvtzu(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fcvtzu(self.ptr, dst.ptr, src.ptr);
    }
    pub fn scvtf(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_scvtf(self.ptr, dst.ptr, src.ptr);
    }
    pub fn ucvtf(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_ucvtf(self.ptr, dst.ptr, src.ptr);
    }
    pub fn fjcvtzs(self: Builder, dst: Reg, src: Reg) void {
        c.luau_a64_asm_fjcvtzs(self.ptr, dst.ptr, src.ptr);
    }

    // ---- FP comparisons --------------------------------------------------

    pub fn fcmp(self: Builder, src1: Reg, src2: Reg) void {
        c.luau_a64_asm_fcmp(self.ptr, src1.ptr, src2.ptr);
    }
    pub fn fcmpz(self: Builder, src: Reg) void {
        c.luau_a64_asm_fcmpz(self.ptr, src.ptr);
    }
    pub fn fcsel(self: Builder, dst: Reg, src1: Reg, src2: Reg, cond: Condition) void {
        c.luau_a64_asm_fcsel(self.ptr, dst.ptr, src1.ptr, src2.ptr, @intFromEnum(cond));
    }

    // ---- Misc ------------------------------------------------------------

    pub fn udf(self: Builder) void {
        c.luau_a64_asm_udf(self.ptr);
    }
    pub fn nop(self: Builder, bytes: u32) void {
        c.luau_a64_asm_nop(self.ptr, bytes);
    }
};
