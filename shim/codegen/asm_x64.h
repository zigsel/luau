// Shim: Luau CodeGen X64 assembler (Luau::CodeGen::X64::AssemblyBuilderX64) —
// the FULL low-level JIT construction kit, with every public instruction and
// method bound.
//
// Unlike the older flat-struct `assembler.h`, this surface models the
// value-type operands (RegisterX64 / OperandX64) and Label as opaque, heap-
// owned handles:
//
//   - LuauX64Reg*     owns a copy of a RegisterX64
//   - LuauX64Operand* owns a copy of an OperandX64
//   - LuauX64Label*   owns a Label (the builder mutates it during placement)
//
// Registers come from named factory fns (luau_x64_reg_rax(), ...) or the
// generic luau_x64_reg(size, index). Operands are built from a register, an
// immediate, or a base/index/scale/disp memory address. Every handle must be
// released with the matching free fn.
//
// Overloaded C++ methods are disambiguated with a numeric suffix.
#pragma once

#include "common.h"
#include "handles.h"

#include <stdint.h>

LUAU_BEGIN_DECLS

// ---- enums (mirror the C++ enums) ---------------------------------------

// SizeX64
typedef enum LuauX64Size {
    LUAU_X64_SIZE_NONE = 0,
    LUAU_X64_SIZE_BYTE = 1,
    LUAU_X64_SIZE_WORD = 2,
    LUAU_X64_SIZE_DWORD = 3,
    LUAU_X64_SIZE_QWORD = 4,
    LUAU_X64_SIZE_XMMWORD = 5,
    LUAU_X64_SIZE_YMMWORD = 6,
} LuauX64Size;

// ConditionX64
typedef enum LuauX64Cond {
    LUAU_X64_COND_OVERFLOW = 0,
    LUAU_X64_COND_NO_OVERFLOW,
    LUAU_X64_COND_CARRY,
    LUAU_X64_COND_NO_CARRY,
    LUAU_X64_COND_BELOW,
    LUAU_X64_COND_BELOW_EQUAL,
    LUAU_X64_COND_ABOVE,
    LUAU_X64_COND_ABOVE_EQUAL,
    LUAU_X64_COND_EQUAL,
    LUAU_X64_COND_LESS,
    LUAU_X64_COND_LESS_EQUAL,
    LUAU_X64_COND_GREATER,
    LUAU_X64_COND_GREATER_EQUAL,
    LUAU_X64_COND_NOT_BELOW,
    LUAU_X64_COND_NOT_BELOW_EQUAL,
    LUAU_X64_COND_NOT_ABOVE,
    LUAU_X64_COND_NOT_ABOVE_EQUAL,
    LUAU_X64_COND_NOT_EQUAL,
    LUAU_X64_COND_NOT_LESS,
    LUAU_X64_COND_NOT_LESS_EQUAL,
    LUAU_X64_COND_NOT_GREATER,
    LUAU_X64_COND_NOT_GREATER_EQUAL,
    LUAU_X64_COND_ZERO,
    LUAU_X64_COND_NOT_ZERO,
    LUAU_X64_COND_PARITY,
    LUAU_X64_COND_NOT_PARITY,
} LuauX64Cond;

// ABIX64
typedef enum LuauX64ABI {
    LUAU_X64_ABI_WINDOWS = 0,
    LUAU_X64_ABI_SYSTEMV = 1,
} LuauX64ABI;

// RoundingModeX64
typedef enum LuauX64RoundingMode {
    LUAU_X64_ROUND_NEAREST_EVEN = 0,
    LUAU_X64_ROUND_NEG_INF = 1,
    LUAU_X64_ROUND_POS_INF = 2,
    LUAU_X64_ROUND_ZERO = 3,
} LuauX64RoundingMode;

// AlignmentDataX64
typedef enum LuauX64AlignmentData {
    LUAU_X64_ALIGN_NOP = 0,
    LUAU_X64_ALIGN_INT3 = 1,
    LUAU_X64_ALIGN_UD2 = 2,
} LuauX64AlignmentData;

// FeaturesX64 bitmask
typedef enum LuauX64Features {
    LUAU_X64_FEATURE_FMA3 = 1 << 0,
    LUAU_X64_FEATURE_AVX = 1 << 1,
} LuauX64Features;

// ---- register handles ----------------------------------------------------

// Generic constructor: RegisterX64{size, index}. Returns a heap handle.
LuauX64Reg* luau_x64_reg(LuauX64Size size, uint8_t index);
LuauX64Reg* luau_x64_reg_clone(const LuauX64Reg* r);
void luau_x64_reg_free(LuauX64Reg* r);

// Size accessors / derivations.
LuauX64Size luau_x64_reg_size(const LuauX64Reg* r);
uint8_t luau_x64_reg_index(const LuauX64Reg* r);
LuauX64Reg* luau_x64_reg_byte(const LuauX64Reg* r);  // byteReg()
LuauX64Reg* luau_x64_reg_word(const LuauX64Reg* r);  // wordReg()
LuauX64Reg* luau_x64_reg_dword(const LuauX64Reg* r); // dwordReg()
LuauX64Reg* luau_x64_reg_qword(const LuauX64Reg* r); // qwordReg()

// Named registers (one factory per canonical RegisterX64 constant).
LuauX64Reg* luau_x64_reg_noreg(void);
LuauX64Reg* luau_x64_reg_rip(void);

LuauX64Reg* luau_x64_reg_al(void);
LuauX64Reg* luau_x64_reg_cl(void);
LuauX64Reg* luau_x64_reg_dl(void);
LuauX64Reg* luau_x64_reg_bl(void);
LuauX64Reg* luau_x64_reg_spl(void);
LuauX64Reg* luau_x64_reg_bpl(void);
LuauX64Reg* luau_x64_reg_sil(void);
LuauX64Reg* luau_x64_reg_dil(void);
LuauX64Reg* luau_x64_reg_r8b(void);
LuauX64Reg* luau_x64_reg_r9b(void);
LuauX64Reg* luau_x64_reg_r10b(void);
LuauX64Reg* luau_x64_reg_r11b(void);
LuauX64Reg* luau_x64_reg_r12b(void);
LuauX64Reg* luau_x64_reg_r13b(void);
LuauX64Reg* luau_x64_reg_r14b(void);
LuauX64Reg* luau_x64_reg_r15b(void);

LuauX64Reg* luau_x64_reg_eax(void);
LuauX64Reg* luau_x64_reg_ecx(void);
LuauX64Reg* luau_x64_reg_edx(void);
LuauX64Reg* luau_x64_reg_ebx(void);
LuauX64Reg* luau_x64_reg_esp(void);
LuauX64Reg* luau_x64_reg_ebp(void);
LuauX64Reg* luau_x64_reg_esi(void);
LuauX64Reg* luau_x64_reg_edi(void);
LuauX64Reg* luau_x64_reg_r8d(void);
LuauX64Reg* luau_x64_reg_r9d(void);
LuauX64Reg* luau_x64_reg_r10d(void);
LuauX64Reg* luau_x64_reg_r11d(void);
LuauX64Reg* luau_x64_reg_r12d(void);
LuauX64Reg* luau_x64_reg_r13d(void);
LuauX64Reg* luau_x64_reg_r14d(void);
LuauX64Reg* luau_x64_reg_r15d(void);

LuauX64Reg* luau_x64_reg_rax(void);
LuauX64Reg* luau_x64_reg_rcx(void);
LuauX64Reg* luau_x64_reg_rdx(void);
LuauX64Reg* luau_x64_reg_rbx(void);
LuauX64Reg* luau_x64_reg_rsp(void);
LuauX64Reg* luau_x64_reg_rbp(void);
LuauX64Reg* luau_x64_reg_rsi(void);
LuauX64Reg* luau_x64_reg_rdi(void);
LuauX64Reg* luau_x64_reg_r8(void);
LuauX64Reg* luau_x64_reg_r9(void);
LuauX64Reg* luau_x64_reg_r10(void);
LuauX64Reg* luau_x64_reg_r11(void);
LuauX64Reg* luau_x64_reg_r12(void);
LuauX64Reg* luau_x64_reg_r13(void);
LuauX64Reg* luau_x64_reg_r14(void);
LuauX64Reg* luau_x64_reg_r15(void);

LuauX64Reg* luau_x64_reg_xmm0(void);
LuauX64Reg* luau_x64_reg_xmm1(void);
LuauX64Reg* luau_x64_reg_xmm2(void);
LuauX64Reg* luau_x64_reg_xmm3(void);
LuauX64Reg* luau_x64_reg_xmm4(void);
LuauX64Reg* luau_x64_reg_xmm5(void);
LuauX64Reg* luau_x64_reg_xmm6(void);
LuauX64Reg* luau_x64_reg_xmm7(void);
LuauX64Reg* luau_x64_reg_xmm8(void);
LuauX64Reg* luau_x64_reg_xmm9(void);
LuauX64Reg* luau_x64_reg_xmm10(void);
LuauX64Reg* luau_x64_reg_xmm11(void);
LuauX64Reg* luau_x64_reg_xmm12(void);
LuauX64Reg* luau_x64_reg_xmm13(void);
LuauX64Reg* luau_x64_reg_xmm14(void);
LuauX64Reg* luau_x64_reg_xmm15(void);

LuauX64Reg* luau_x64_reg_ymm0(void);
LuauX64Reg* luau_x64_reg_ymm1(void);
LuauX64Reg* luau_x64_reg_ymm2(void);
LuauX64Reg* luau_x64_reg_ymm3(void);
LuauX64Reg* luau_x64_reg_ymm4(void);
LuauX64Reg* luau_x64_reg_ymm5(void);
LuauX64Reg* luau_x64_reg_ymm6(void);
LuauX64Reg* luau_x64_reg_ymm7(void);
LuauX64Reg* luau_x64_reg_ymm8(void);
LuauX64Reg* luau_x64_reg_ymm9(void);
LuauX64Reg* luau_x64_reg_ymm10(void);
LuauX64Reg* luau_x64_reg_ymm11(void);
LuauX64Reg* luau_x64_reg_ymm12(void);
LuauX64Reg* luau_x64_reg_ymm13(void);
LuauX64Reg* luau_x64_reg_ymm14(void);
LuauX64Reg* luau_x64_reg_ymm15(void);

// ---- operand handles -----------------------------------------------------

// OperandX64 from a register (CategoryX64::reg).
LuauX64Operand* luau_x64_op_reg(const LuauX64Reg* r);
// OperandX64 from a 32-bit immediate (CategoryX64::imm).
LuauX64Operand* luau_x64_imm(int32_t imm);
// OperandX64 memory: [base + index*scale + disp] with access `size`
// (CategoryX64::mem). base / index may be NULL (treated as noreg). scale 1/2/4/8.
LuauX64Operand* luau_x64_op_mem(
    LuauX64Size size,
    const LuauX64Reg* index,
    uint8_t scale,
    const LuauX64Reg* base,
    int32_t disp);
LuauX64Operand* luau_x64_op_clone(const LuauX64Operand* o);
void luau_x64_op_free(LuauX64Operand* o);

// ---- label handles -------------------------------------------------------

LuauX64Label* luau_x64_label_new(void);
void luau_x64_label_free(LuauX64Label* l);
uint32_t luau_x64_label_id(const LuauX64Label* l);
uint32_t luau_x64_label_location(const LuauX64Label* l);

// ---- builder lifetime ----------------------------------------------------

// Two constructors. _abi takes an explicit ABI; the plain one uses the default.
LuauX64Asm* luau_x64_asm_new_abi(int log_text, LuauX64ABI abi, unsigned int features);
LuauX64Asm* luau_x64_asm_new(int log_text, unsigned int features);
void luau_x64_asm_free(LuauX64Asm* a);

// ---- base two-operand instructions --------------------------------------

void luau_x64_asm_add(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_sub(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_cmp(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_and(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_or(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_xor(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);

// ---- shifts --------------------------------------------------------------

void luau_x64_asm_sal(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_sar(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_shl(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_shr(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_rol(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_ror(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);

// ---- mov family ----------------------------------------------------------

void luau_x64_asm_mov(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_mov64(LuauX64Asm* a, const LuauX64Reg* lhs, int64_t imm);
void luau_x64_asm_movsx(LuauX64Asm* a, const LuauX64Reg* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_movzx(LuauX64Asm* a, const LuauX64Reg* lhs, const LuauX64Operand* rhs);

// ---- one-operand instructions -------------------------------------------

void luau_x64_asm_div(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_idiv(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_mul(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_imul1(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_neg(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_not(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_dec(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_inc(LuauX64Asm* a, const LuauX64Operand* op);

// Additional imul forms.
void luau_x64_asm_imul2(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_imul3(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* lhs, int32_t rhs);

void luau_x64_asm_test(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_lea(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);
void luau_x64_asm_setcc(LuauX64Asm* a, LuauX64Cond cond, const LuauX64Operand* op);
void luau_x64_asm_cmov(LuauX64Asm* a, LuauX64Cond cond, const LuauX64Reg* lhs, const LuauX64Operand* rhs);

void luau_x64_asm_push(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_pop(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_ret(LuauX64Asm* a);

// ---- control flow --------------------------------------------------------

void luau_x64_asm_jcc(LuauX64Asm* a, LuauX64Cond cond, LuauX64Label* label);
void luau_x64_asm_jmp_label(LuauX64Asm* a, LuauX64Label* label);
void luau_x64_asm_jmp_op(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_call_label(LuauX64Asm* a, LuauX64Label* label);
void luau_x64_asm_call_op(LuauX64Asm* a, const LuauX64Operand* op);
void luau_x64_asm_lea_label(LuauX64Asm* a, const LuauX64Reg* lhs, LuauX64Label* label);

void luau_x64_asm_int3(LuauX64Asm* a);
void luau_x64_asm_ud2(LuauX64Asm* a);
void luau_x64_asm_cqo(LuauX64Asm* a);
void luau_x64_asm_cdq(LuauX64Asm* a);

void luau_x64_asm_bsr(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Operand* src);
void luau_x64_asm_bsf(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Operand* src);
void luau_x64_asm_bswap(LuauX64Asm* a, const LuauX64Reg* dst);

// ---- alignment -----------------------------------------------------------

void luau_x64_asm_nop(LuauX64Asm* a, uint32_t length);
void luau_x64_asm_align(LuauX64Asm* a, uint32_t alignment, LuauX64AlignmentData data);

// ---- AVX -----------------------------------------------------------------

void luau_x64_asm_vaddpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vaddps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vaddsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vaddss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vsubsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vsubss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vsubps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmulsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmulss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmulps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vdivsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vdivss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vdivps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vandps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vandpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vandnpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vxorps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vxorpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vorps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vorpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vucomisd(LuauX64Asm* a, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vucomiss(LuauX64Asm* a, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vcvttsd2si(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vcvtsi2sd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcvtsi2ss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcvtsd2ss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcvtss2sd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vroundsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, LuauX64RoundingMode mode);
void luau_x64_asm_vroundss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, LuauX64RoundingMode mode);
void luau_x64_asm_vroundps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src, LuauX64RoundingMode mode);

void luau_x64_asm_vsqrtpd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vsqrtps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vsqrtsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vsqrtss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vmovsd2(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovsd3(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmovss2(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovss3(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmovapd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovaps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovupd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovups(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src);
void luau_x64_asm_vmovq(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs);

void luau_x64_asm_vmaxps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmaxsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vmaxss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vminps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vminsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vminss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vcmpeqsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcmpltsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcmpltss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vcmpeqps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

void luau_x64_asm_vblendvps(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, const LuauX64Reg* mask);
void luau_x64_asm_vblendvpd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, const LuauX64Reg* mask);

void luau_x64_asm_vpshufps(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, uint8_t shuffle);
void luau_x64_asm_vpinsrd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, uint8_t offset);
void luau_x64_asm_vpextrd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src, uint8_t offset);

void luau_x64_asm_vdpps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, uint8_t mask);
void luau_x64_asm_vfmadd213ps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);
void luau_x64_asm_vfmadd213pd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2);

// ---- labels (builder side) ----------------------------------------------

// setLabel(): places a fresh label at the current location, writing it into
// `out` (a label handle the caller owns) and returning that handle.
void luau_x64_asm_set_label_here(LuauX64Asm* a, LuauX64Label* out);
// setLabel(Label&): binds an existing label to the current location.
void luau_x64_asm_place_label(LuauX64Asm* a, LuauX64Label* label);
// getLabelOffset(): byte offset of a placed label.
uint32_t luau_x64_asm_label_offset(LuauX64Asm* a, const LuauX64Label* label);

// ---- constant allocation (rip-relative) ---------------------------------

LuauX64Operand* luau_x64_asm_i32(LuauX64Asm* a, int32_t value);
LuauX64Operand* luau_x64_asm_i64(LuauX64Asm* a, int64_t value);
LuauX64Operand* luau_x64_asm_f32(LuauX64Asm* a, float value);
LuauX64Operand* luau_x64_asm_f64(LuauX64Asm* a, double value);
LuauX64Operand* luau_x64_asm_u32x4(LuauX64Asm* a, uint32_t x, uint32_t y, uint32_t z, uint32_t w);
LuauX64Operand* luau_x64_asm_f32x4(LuauX64Asm* a, float x, float y, float z, float w);
LuauX64Operand* luau_x64_asm_f64x2(LuauX64Asm* a, double x, double y);
LuauX64Operand* luau_x64_asm_bytes(LuauX64Asm* a, const void* ptr, size_t size, size_t align);

// ---- finalize / output ---------------------------------------------------

int luau_x64_asm_finalize(LuauX64Asm* a);
uint32_t luau_x64_asm_code_size(LuauX64Asm* a);
unsigned int luau_x64_asm_instruction_count(LuauX64Asm* a);
const uint8_t* luau_x64_asm_code_ptr(LuauX64Asm* a, size_t* out_len);
const uint8_t* luau_x64_asm_data_ptr(LuauX64Asm* a, size_t* out_len);
// Textual disassembly log; caller frees with free(). NULL if logging is off.
char* luau_x64_asm_text(LuauX64Asm* a);

// Append to the text log (printf-style, pre-formatted by the caller).
void luau_x64_asm_log_append(LuauX64Asm* a, const char* text);

LUAU_END_DECLS
