// Shim: Luau CodeGen IR (IrBuilder / IrData) — the JIT intermediate
// representation construction + inspection kit.
//
// IrBuilder constructs IR for a function. The high-level entry points
// (buildFunctionIr / translateInst / rebuildBytecodeBasicBlocks) take a
// `Proto*`, which is a VM-internal type we cannot synthesize here without VM
// internals — those are intentionally NOT bound (see comments in ir.cpp).
//
// What IS bound: the operand/constant/instruction/block factory methods that
// build IR independently of a Proto (constInt, constDouble, inst, block,
// vmReg, ...), plus read-only inspection of the resulting IrFunction (constants,
// instructions, their commands and operands). This is enough to assemble and
// inspect IR graphs by hand.
//
// An IrOp is exposed as a packed uint32_t with the same layout as the C++
// bitfield struct IrOp{ kind:4, index:28 } — kind in the low 4 bits, index in
// the high 28. Use luau_irop_kind / luau_irop_index to unpack.
#pragma once

#include "common.h"
#include "handles.h"

#include <stdint.h>

LUAU_BEGIN_DECLS

// Packed IrOp: bits[0..3] = kind (LuauIrOpKind), bits[4..31] = index.
typedef uint32_t LuauIrOp;

// Mirrors Luau::CodeGen::IrOpKind.
typedef enum LuauIrOpKind {
    LUAU_IR_OP_NONE = 0,
    LUAU_IR_OP_UNDEF = 1,
    LUAU_IR_OP_CONSTANT = 2,
    LUAU_IR_OP_CONDITION = 3,
    LUAU_IR_OP_INST = 4,
    LUAU_IR_OP_BLOCK = 5,
    LUAU_IR_OP_VMREG = 6,
    LUAU_IR_OP_VMCONST = 7,
    LUAU_IR_OP_VMUPVALUE = 8,
    LUAU_IR_OP_VMEXIT = 9,
} LuauIrOpKind;

// Mirrors Luau::CodeGen::IrConstKind.
typedef enum LuauIrConstKind {
    LUAU_IR_CONST_INT = 0,
    LUAU_IR_CONST_INT64 = 1,
    LUAU_IR_CONST_UINT = 2,
    LUAU_IR_CONST_DOUBLE = 3,
    LUAU_IR_CONST_TAG = 4,
    LUAU_IR_CONST_IMPORT = 5,
} LuauIrConstKind;

// Mirrors Luau::CodeGen::IrCondition.
typedef enum LuauIrCondition {
    LUAU_IR_COND_EQUAL = 0,
    LUAU_IR_COND_NOT_EQUAL,
    LUAU_IR_COND_LESS,
    LUAU_IR_COND_NOT_LESS,
    LUAU_IR_COND_LESS_EQUAL,
    LUAU_IR_COND_NOT_LESS_EQUAL,
    LUAU_IR_COND_GREATER,
    LUAU_IR_COND_NOT_GREATER,
    LUAU_IR_COND_GREATER_EQUAL,
    LUAU_IR_COND_NOT_GREATER_EQUAL,
    LUAU_IR_COND_UNSIGNED_LESS,
    LUAU_IR_COND_UNSIGNED_LESS_EQUAL,
    LUAU_IR_COND_UNSIGNED_GREATER,
    LUAU_IR_COND_UNSIGNED_GREATER_EQUAL,
} LuauIrCondition;

// Mirrors Luau::CodeGen::IrBlockKind.
typedef enum LuauIrBlockKind {
    LUAU_IR_BLOCK_BYTECODE = 0,
    LUAU_IR_BLOCK_FALLBACK = 1,
    LUAU_IR_BLOCK_INTERNAL = 2,
    LUAU_IR_BLOCK_LINEARIZED = 3,
    LUAU_IR_BLOCK_EXITSYNC = 4,
    LUAU_IR_BLOCK_DEAD = 5,
} LuauIrBlockKind;

// Unpack a packed LuauIrOp.
LuauIrOpKind luau_irop_kind(LuauIrOp op);
uint32_t luau_irop_index(LuauIrOp op);

// ---- builder lifetime ----------------------------------------------------

// Create an IrBuilder with default (no-op) host hooks. The builder starts with
// an empty IrFunction; you can populate it with the factory methods below.
// NOTE: to lower real Luau bytecode you would call buildFunctionIr(Proto*),
// which requires a VM-internal Proto and is therefore not exposed here.
LuauIrBuilder* luau_irbuilder_new(void);
void luau_irbuilder_free(LuauIrBuilder* b);

// ---- constant / operand factories (no Proto required) --------------------

LuauIrOp luau_irbuilder_undef(LuauIrBuilder* b);
LuauIrOp luau_irbuilder_const_int(LuauIrBuilder* b, int value);
LuauIrOp luau_irbuilder_const_int64(LuauIrBuilder* b, int64_t value);
LuauIrOp luau_irbuilder_const_uint(LuauIrBuilder* b, unsigned int value);
LuauIrOp luau_irbuilder_const_double(LuauIrBuilder* b, double value);
LuauIrOp luau_irbuilder_const_tag(LuauIrBuilder* b, uint8_t value);
LuauIrOp luau_irbuilder_const_import(LuauIrBuilder* b, unsigned int value);
LuauIrOp luau_irbuilder_cond(LuauIrBuilder* b, LuauIrCondition cond);

LuauIrOp luau_irbuilder_vmreg(LuauIrBuilder* b, uint8_t index);
LuauIrOp luau_irbuilder_vmconst(LuauIrBuilder* b, uint32_t index);
LuauIrOp luau_irbuilder_vmupvalue(LuauIrBuilder* b, uint8_t index);
LuauIrOp luau_irbuilder_vmexit(LuauIrBuilder* b, uint32_t pcpos);

// constAny: a constant of the given IrConstKind with the raw bit pattern in
// `asCommonKey` (for Double, this is the IEEE-754 bits of the double). Mirrors
// IrBuilder::constAny(IrConst, uint64_t).
LuauIrOp luau_irbuilder_const_any(LuauIrBuilder* b, LuauIrConstKind kind, uint64_t bits, uint64_t as_common_key);

// A new basic block of the requested kind (returns a Block-kind LuauIrOp).
LuauIrOp luau_irbuilder_block(LuauIrBuilder* b, LuauIrBlockKind kind);
// A block whose instruction range begins at the given instruction index.
LuauIrOp luau_irbuilder_block_at_inst(LuauIrBuilder* b, uint32_t index);
// A fallback block for the given bytecode pc position.
LuauIrOp luau_irbuilder_fallback_block(LuauIrBuilder* b, uint32_t pcpos);
// Begin emitting into a block created by luau_irbuilder_block.
void luau_irbuilder_begin_block(LuauIrBuilder* b, LuauIrOp block);
// True (1) if the given Block-kind operand refers to an Internal-kind block.
int luau_irbuilder_is_internal_block(LuauIrBuilder* b, LuauIrOp block);

// Emit a guarded tag load+check: load tag at `loc` and branch to `fallback` if
// it is not `tag`. Mirrors IrBuilder::loadAndCheckTag.
void luau_irbuilder_load_and_check_tag(LuauIrBuilder* b, LuauIrOp loc, uint8_t tag, LuauIrOp fallback);
// Emit a "safe environment" check for the given bytecode pc position.
void luau_irbuilder_check_safe_env(LuauIrBuilder* b, int pcpos);

// Emit an instruction. `cmd` is the raw Luau::CodeGen::IrCmd enum value (the
// IrCmd enum is large and volatile; pass the integer value directly). Up to 7
// operands (the maximum any IrCmd uses); pass a NONE-kind LuauIrOp (value 0)
// for unused slots. Returns the instruction result as an Inst-kind LuauIrOp.
LuauIrOp luau_irbuilder_inst0(LuauIrBuilder* b, uint8_t cmd);
LuauIrOp luau_irbuilder_inst1(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a);
LuauIrOp luau_irbuilder_inst2(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c);
LuauIrOp luau_irbuilder_inst3(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d);
LuauIrOp luau_irbuilder_inst4(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e);
LuauIrOp luau_irbuilder_inst5(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f);
LuauIrOp luau_irbuilder_inst6(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f, LuauIrOp g);
LuauIrOp luau_irbuilder_inst7(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f, LuauIrOp g, LuauIrOp h);
// Emit an instruction with an arbitrary operand count from an array of packed
// operands. Mirrors IrBuilder::inst(IrCmd, const IrOps&).
LuauIrOp luau_irbuilder_inst_n(LuauIrBuilder* b, uint8_t cmd, const LuauIrOp* ops, size_t count);

// ---- IrFunction inspection ----------------------------------------------

size_t luau_irbuilder_const_count(LuauIrBuilder* b);
size_t luau_irbuilder_inst_count(LuauIrBuilder* b);
size_t luau_irbuilder_block_count(LuauIrBuilder* b);

// Constant inspection by index. luau_irbuilder_const_kind returns -1 if out of
// range. The typed getters are only meaningful for the matching kind.
int luau_irbuilder_const_kind(LuauIrBuilder* b, size_t idx);
int luau_irbuilder_const_get_int(LuauIrBuilder* b, size_t idx);
int64_t luau_irbuilder_const_get_int64(LuauIrBuilder* b, size_t idx);
unsigned int luau_irbuilder_const_get_uint(LuauIrBuilder* b, size_t idx);
double luau_irbuilder_const_get_double(LuauIrBuilder* b, size_t idx);
uint8_t luau_irbuilder_const_get_tag(LuauIrBuilder* b, size_t idx);

// Raw 64-bit bit pattern of the constant at `idx` (the union storage), 0 if out
// of range. Useful for reading Double bits without float conversion.
uint64_t luau_irbuilder_const_get_bits(LuauIrBuilder* b, size_t idx);

// Instruction inspection by index. Returns the raw IrCmd value, or -1 if out of
// range.
int luau_irbuilder_inst_cmd(LuauIrBuilder* b, size_t idx);
// Number of operands on the instruction at `idx`.
size_t luau_irbuilder_inst_op_count(LuauIrBuilder* b, size_t idx);
// The `op_idx`-th operand of instruction `idx` as a packed LuauIrOp
// (NONE-kind / 0 if out of range).
LuauIrOp luau_irbuilder_inst_op(LuauIrBuilder* b, size_t idx, size_t op_idx);
// Liveness bookkeeping on the instruction at `idx` (0 if out of range).
uint32_t luau_irbuilder_inst_last_use(LuauIrBuilder* b, size_t idx);
uint32_t luau_irbuilder_inst_use_count(LuauIrBuilder* b, size_t idx);

// ---- IrBlock inspection (per-block) --------------------------------------

// Kind of the block at `idx` as LuauIrBlockKind, or -1 if out of range.
int luau_irbuilder_block_kind(LuauIrBuilder* b, size_t idx);
// Flag bits (kBlockFlag*), use count, sort/chain keys.
uint8_t luau_irbuilder_block_flags(LuauIrBuilder* b, size_t idx);
uint16_t luau_irbuilder_block_use_count(LuauIrBuilder* b, size_t idx);
// Inclusive [start, finish] instruction-index range of the block; ~0u sentinels
// if unset / out of range.
uint32_t luau_irbuilder_block_start(LuauIrBuilder* b, size_t idx);
uint32_t luau_irbuilder_block_finish(LuauIrBuilder* b, size_t idx);
// Bytecode pc position at which the block was generated (kBlockNoStartPc / ~0u
// if none).
uint32_t luau_irbuilder_block_startpc(LuauIrBuilder* b, size_t idx);
// Number of instructions belonging to the block at `idx`, derived from its
// [start, finish] range (0 if the range is unset or out of range).
size_t luau_irbuilder_block_inst_count(LuauIrBuilder* b, size_t idx);
// The `n`-th instruction index that belongs to block `idx` (i.e. start + n),
// or ~0u if out of range. Feed the result into luau_irbuilder_inst_* getters.
uint32_t luau_irbuilder_block_inst_at(LuauIrBuilder* b, size_t idx, size_t n);

// ---- IrFunction / builder state ------------------------------------------

// Entry block index of the built function.
uint32_t luau_irbuilder_entry_block(LuauIrBuilder* b);
// True (1) if the active block has already been terminated.
int luau_irbuilder_in_terminated_block(LuauIrBuilder* b);
// Index of the block currently being emitted into, or ~0u if none.
uint32_t luau_irbuilder_active_block_idx(LuauIrBuilder* b);

LUAU_END_DECLS
