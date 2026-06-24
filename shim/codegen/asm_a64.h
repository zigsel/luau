// Shim: Luau CodeGen AArch64 ASSEMBLER (Luau::CodeGen::A64::AssemblyBuilderA64)
// — the low-level arm64 JIT construction kit. Exposes EVERY public instruction
// emit method on AssemblyBuilderA64 plus its lifetime / label / output API.
//
// Value-type operands are modeled as opaque handles that OWN a heap copy of the
// underlying C++ value:
//   LuauA64Reg*      owns a RegisterA64
//   LuauA64Address*  owns an AddressA64
// Build these with the factory functions, pass them to emit methods (which
// dereference them by value), and free them with luau_a64_reg_free /
// luau_a64_address_free. Labels are referenced by uint32_t id (Label::id).
#pragma once

#include "common.h"
#include "handles.h"

#include <stdint.h>

LUAU_BEGIN_DECLS

// Register kind, mirrors Luau::CodeGen::A64::KindA64.
typedef enum LuauA64Kind {
    LUAU_A64_KIND_NONE = 0,
    LUAU_A64_KIND_W = 1, // 32-bit GPR
    LUAU_A64_KIND_X = 2, // 64-bit GPR
    LUAU_A64_KIND_S = 3, // 32-bit SIMD&FP scalar
    LUAU_A64_KIND_D = 4, // 64-bit SIMD&FP scalar
    LUAU_A64_KIND_Q = 5, // 128-bit SIMD&FP vector
} LuauA64Kind;

// Condition code, mirrors Luau::CodeGen::A64::ConditionA64.
typedef enum LuauA64Condition {
    LUAU_A64_COND_EQUAL = 0,
    LUAU_A64_COND_NOT_EQUAL,
    LUAU_A64_COND_CARRY_SET,
    LUAU_A64_COND_CARRY_CLEAR,
    LUAU_A64_COND_MINUS,
    LUAU_A64_COND_PLUS,
    LUAU_A64_COND_OVERFLOW,
    LUAU_A64_COND_NO_OVERFLOW,
    LUAU_A64_COND_UNSIGNED_GREATER,
    LUAU_A64_COND_UNSIGNED_LESS_EQUAL,
    LUAU_A64_COND_GREATER_EQUAL,
    LUAU_A64_COND_LESS,
    LUAU_A64_COND_GREATER,
    LUAU_A64_COND_LESS_EQUAL,
    LUAU_A64_COND_ALWAYS,
} LuauA64Condition;

// Address kind, mirrors Luau::CodeGen::A64::AddressKindA64.
typedef enum LuauA64AddressKind {
    LUAU_A64_ADDR_REG = 0,  // reg + reg
    LUAU_A64_ADDR_IMM = 1,  // reg + imm
    LUAU_A64_ADDR_PRE = 2,  // reg + imm, reg += imm
    LUAU_A64_ADDR_POST = 3, // reg, reg += imm
} LuauA64AddressKind;

// FeaturesA64, mirrors the builder's features bitmask.
typedef enum LuauA64Feature {
    LUAU_A64_FEATURE_JSCVT = 1 << 0,
    LUAU_A64_FEATURE_ADVSIMD = 1 << 1,
} LuauA64Feature;

// ---- register operand handles -------------------------------------------

// Build a register handle from (kind, index). index in [0, 31]. For X/W index
// 31 is the zero register (xzr/wzr); use luau_a64_reg_sp for the stack pointer.
// Returns NULL on allocation failure. Free with luau_a64_reg_free.
LuauA64Reg* luau_a64_reg(LuauA64Kind kind, int index);
// The stack pointer (sp): KindA64::none, index 31.
LuauA64Reg* luau_a64_reg_sp(void);
LuauA64Reg* luau_a64_reg_noreg(void);
// castReg: reinterpret an existing register as another kind (same index).
LuauA64Reg* luau_a64_reg_cast(LuauA64Kind kind, const LuauA64Reg* reg);
void luau_a64_reg_free(LuauA64Reg* r);
LuauA64Kind luau_a64_reg_kind(const LuauA64Reg* r);
int luau_a64_reg_index(const LuauA64Reg* r);

// ---- address operand handles --------------------------------------------

// reg + imm (kind imm/pre/post). base must be an X register or sp.
LuauA64Address* luau_a64_address_imm(const LuauA64Reg* base, int offset, LuauA64AddressKind kind);
// reg + reg. base and offset must be X registers.
LuauA64Address* luau_a64_address_reg(const LuauA64Reg* base, const LuauA64Reg* offset);
void luau_a64_address_free(LuauA64Address* a);

// ---- lifetime ------------------------------------------------------------

// Create a builder. `log_text` enables textual disassembly; `features` is a
// bitmask of LuauA64Feature. Returns NULL on failure. Free with luau_a64_asm_free.
LuauAsmA64* luau_a64_asm_new(int log_text, unsigned int features);
void luau_a64_asm_free(LuauAsmA64* a);

// ---- labels --------------------------------------------------------------

// Place a fresh label at the current location; returns its id.
uint32_t luau_a64_asm_set_label_here(LuauAsmA64* a);
// Allocate a label id without placing it.
uint32_t luau_a64_asm_make_label(LuauAsmA64* a);
// Bind a previously-made label id to the current location.
void luau_a64_asm_place_label(LuauAsmA64* a, uint32_t label_id);
// Byte offset of a placed label (valid after placement / finalize).
uint32_t luau_a64_asm_label_offset(LuauAsmA64* a, uint32_t label_id);

// ---- finalize / output ---------------------------------------------------

int luau_a64_asm_finalize(LuauAsmA64* a);
// Code size in bytes.
uint32_t luau_a64_asm_code_size(LuauAsmA64* a);
unsigned int luau_a64_asm_instruction_count(LuauAsmA64* a);
// Emitted code: pointer to the uint32_t instruction words and *byte* length.
// (A64 code is measured in 32-bit words; out_len is in bytes.) out_len may be NULL.
const uint8_t* luau_a64_asm_code_ptr(LuauAsmA64* a, size_t* out_len);
// Number of 32-bit instruction words.
size_t luau_a64_asm_code_word_count(LuauAsmA64* a);
// rip-relative constant data blob (bytes) and its length. out_len may be NULL.
const uint8_t* luau_a64_asm_data_ptr(LuauAsmA64* a, size_t* out_len);
// Textual disassembly; caller frees with free(). NULL if logging off / OOM.
char* luau_a64_asm_get_text(LuauAsmA64* a);
// Append a literal string to the log.
void luau_a64_asm_log_append(LuauAsmA64* a, const char* text);

// ---- static helpers ------------------------------------------------------

int luau_a64_asm_is_mask_supported(uint32_t mask);
int luau_a64_asm_is_fmov_supported_fp64(double value);
int luau_a64_asm_is_fmov_supported_fp32(float value);

// ---- Moves ---------------------------------------------------------------

void luau_a64_asm_mov(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_mov_imm(LuauAsmA64* a, const LuauA64Reg* dst, int src);
void luau_a64_asm_movz(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift);
void luau_a64_asm_movn(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift);
void luau_a64_asm_movk(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift);

// ---- Arithmetic ----------------------------------------------------------

void luau_a64_asm_add(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_add_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint16_t src2);
void luau_a64_asm_sub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_sub_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint16_t src2);
void luau_a64_asm_neg(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_mul(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_msub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, const LuauA64Reg* src3);
void luau_a64_asm_sdiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_udiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_rem(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);

// ---- Comparisons ---------------------------------------------------------

void luau_a64_asm_cmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_cmp_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint16_t src2);
void luau_a64_asm_ccmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond, uint8_t nzcv);
void luau_a64_asm_ccmn(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond, uint8_t nzcv);
void luau_a64_asm_ccmn_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint8_t src2, LuauA64Condition cond, uint8_t nzcv);
void luau_a64_asm_cmn_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint16_t src2);
void luau_a64_asm_csel(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond);
void luau_a64_asm_cset(LuauAsmA64* a, const LuauA64Reg* dst, LuauA64Condition cond);

// ---- Bitwise -------------------------------------------------------------

void luau_a64_asm_and(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_orr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_eor(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_bic(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_tst(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift);
void luau_a64_asm_mvn(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);

void luau_a64_asm_and_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2);
void luau_a64_asm_orr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2);
void luau_a64_asm_eor_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2);
void luau_a64_asm_tst_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint32_t src2);

// ---- Shifts --------------------------------------------------------------

void luau_a64_asm_lsl(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_lsr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_asr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_ror(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_clz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_rbit(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_rev(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);

void luau_a64_asm_lsl_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2);
void luau_a64_asm_lsr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2);
void luau_a64_asm_asr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2);
void luau_a64_asm_ror_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2);

// ---- Bitfields -----------------------------------------------------------

void luau_a64_asm_ubfiz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w);
void luau_a64_asm_ubfx(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w);
void luau_a64_asm_sbfiz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w);
void luau_a64_asm_sbfx(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w);

// ---- Loads ---------------------------------------------------------------

void luau_a64_asm_ldr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldrb(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldrh(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldrsb(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldrsh(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldrsw(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src);
void luau_a64_asm_ldp(LuauAsmA64* a, const LuauA64Reg* dst1, const LuauA64Reg* dst2, const LuauA64Address* src);

// ---- Stores --------------------------------------------------------------

void luau_a64_asm_str(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst);
void luau_a64_asm_strb(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst);
void luau_a64_asm_strh(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst);
void luau_a64_asm_stp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, const LuauA64Address* dst);

// ---- Control flow --------------------------------------------------------

void luau_a64_asm_b(LuauAsmA64* a, uint32_t label_id);
void luau_a64_asm_bl(LuauAsmA64* a, uint32_t label_id);
void luau_a64_asm_br(LuauAsmA64* a, const LuauA64Reg* src);
void luau_a64_asm_blr(LuauAsmA64* a, const LuauA64Reg* src);
void luau_a64_asm_ret(LuauAsmA64* a);

void luau_a64_asm_b_cond(LuauAsmA64* a, LuauA64Condition cond, uint32_t label_id);
void luau_a64_asm_cbz(LuauAsmA64* a, const LuauA64Reg* src, uint32_t label_id);
void luau_a64_asm_cbnz(LuauAsmA64* a, const LuauA64Reg* src, uint32_t label_id);
void luau_a64_asm_tbz(LuauAsmA64* a, const LuauA64Reg* src, uint8_t bit, uint32_t label_id);
void luau_a64_asm_tbnz(LuauAsmA64* a, const LuauA64Reg* src, uint8_t bit, uint32_t label_id);

// ---- Address of embedded data / code ------------------------------------

void luau_a64_asm_adr_data(LuauAsmA64* a, const LuauA64Reg* dst, const void* ptr, size_t size);
void luau_a64_asm_adr_u64(LuauAsmA64* a, const LuauA64Reg* dst, uint64_t value);
void luau_a64_asm_adr_f32(LuauAsmA64* a, const LuauA64Reg* dst, float value);
void luau_a64_asm_adr_f64(LuauAsmA64* a, const LuauA64Reg* dst, double value);
void luau_a64_asm_adr_label(LuauAsmA64* a, const LuauA64Reg* dst, uint32_t label_id);

// ---- Floating-point / SIMD moves ----------------------------------------

void luau_a64_asm_fmov(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fmov_f64(LuauAsmA64* a, const LuauA64Reg* dst, double src);
void luau_a64_asm_fmov_f32(LuauAsmA64* a, const LuauA64Reg* dst, float src);

// ---- Floating-point / SIMD math -----------------------------------------

void luau_a64_asm_fabs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fadd(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_fdiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_fmul(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_fneg(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fsqrt(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fsub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_faddp(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fmla(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);

// ---- Vector component manipulation --------------------------------------

void luau_a64_asm_ins_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index);
void luau_a64_asm_ins_4s_idx(LuauAsmA64* a, const LuauA64Reg* dst, uint8_t dstIndex, const LuauA64Reg* src, uint8_t srcIndex);
void luau_a64_asm_dup_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index);
void luau_a64_asm_umov_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index);

void luau_a64_asm_fcmeq_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_fcmgt_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_bit(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, const LuauA64Reg* mask);
void luau_a64_asm_bif(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, const LuauA64Reg* mask);

// ---- FP rounding and conversions ----------------------------------------

void luau_a64_asm_frinta(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_frintm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_frintp(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fcvt(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fcvtzs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fcvtzu(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_scvtf(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_ucvtf(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);
void luau_a64_asm_fjcvtzs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src);

// ---- FP comparisons ------------------------------------------------------

void luau_a64_asm_fcmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2);
void luau_a64_asm_fcmpz(LuauAsmA64* a, const LuauA64Reg* src);
void luau_a64_asm_fcsel(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond);

// ---- Misc ----------------------------------------------------------------

void luau_a64_asm_udf(LuauAsmA64* a);
void luau_a64_asm_nop(LuauAsmA64* a, uint32_t bytes);

LUAU_END_DECLS
