// Shim: Luau Bytecode graph (Luau::Bytecode namespace, BytecodeGraph.h).
//
// Wraps fromFunctionBytecode()/toFunctionBytecode() and read-only inspection of
// the resulting CompTimeBcFunction (a single function's bytecode lifted into an
// SSA-ish graph of blocks/instructions/constants/immediates/phis/projections).
//
// The graph types use a template (BcFunction<VmConst>); only the comp-time
// instantiation CompTimeBcFunction = BcFunction<BcVmConst> is parsed from
// bytecode by the upstream API, so that is what we expose. Container fields are
// exposed as count + at(i) accessors. Nested helper objects that only make sense
// inside the C++ traversal (BcRef<T>, RegMap, the as<T>() typed views, the std::list
// block instruction ordering iterators, the SmallVector internals) are NOT exposed;
// the observable scalar/index data they wrap is reachable through the accessors here.
#pragma once

#include "common.h"
#include "handles.h"

#include <stdint.h>

LUAU_BEGIN_DECLS

// BcOp kinds (Luau::Bytecode::BcOpKind) returned as int.
enum {
    LUAU_BCOP_NONE = 0,
    LUAU_BCOP_IMM,
    LUAU_BCOP_INST,
    LUAU_BCOP_BLOCK,
    LUAU_BCOP_PHI,
    LUAU_BCOP_PROJ,
    LUAU_BCOP_VMREG,
    LUAU_BCOP_VMCONST,
    LUAU_BCOP_VMUPVALUE,
    LUAU_BCOP_VMPROTO
};

// BcImmKind (Luau::Bytecode::BcImmKind).
enum {
    LUAU_BCIMM_BOOLEAN = 0,
    LUAU_BCIMM_INT,
    LUAU_BCIMM_IMPORT
};

// BcVmConstKind (Luau::Bytecode::BcVmConstKind).
enum {
    LUAU_BCVMCONST_NIL = 0,
    LUAU_BCVMCONST_BOOLEAN,
    LUAU_BCVMCONST_NUMBER,
    LUAU_BCVMCONST_VECTOR,
    LUAU_BCVMCONST_STRING,
    LUAU_BCVMCONST_IMPORT,
    LUAU_BCVMCONST_TABLE,
    LUAU_BCVMCONST_CLOSURE,
    LUAU_BCVMCONST_INTEGER
};

// BcBlockEdgeKind (Luau::Bytecode::BcBlockEdgeKind).
enum {
    LUAU_BCEDGE_BRANCH = 0,
    LUAU_BCEDGE_FALLTHROUGH,
    LUAU_BCEDGE_LOOP
};

// Parse a single function's bytecode into a graph. `strings` is the string table
// (count + at) that backs string_view constants/names in the result; it must
// outlive the returned graph (the graph borrows those views). Returns NULL if
// the bytecode could not be parsed (the upstream optional was empty) or on error.
//
// strings are passed as parallel arrays of pointer/length.
LuauBcGraph* luau_bcg_from_function_bytecode(const char* bytecode, size_t bytecode_len,
                                             const char* const* string_ptrs,
                                             const size_t* string_lens,
                                             size_t string_count);
void luau_bcg_free(LuauBcGraph* g);

// Serialize the graph back to a function bytecode blob (owned by the handle,
// valid until the next to_bytecode call or free). Returns NULL on error.
const char* luau_bcg_to_function_bytecode(LuauBcGraph* g, size_t* out_len);

// --- function-level scalars ---
unsigned char luau_bcg_max_stack_size(LuauBcGraph* g);
unsigned char luau_bcg_num_params(LuauBcGraph* g);
unsigned char luau_bcg_num_upvalues(LuauBcGraph* g);
int luau_bcg_is_vararg(LuauBcGraph* g);
unsigned char luau_bcg_flags(LuauBcGraph* g);
unsigned int luau_bcg_line_defined(LuauBcGraph* g);
const char* luau_bcg_debug_name(LuauBcGraph* g, size_t* out_len);

// entry/exit blocks: returns the block index, or -1 if the op is not a Block.
int luau_bcg_entry_block(LuauBcGraph* g);
int luau_bcg_exit_block(LuauBcGraph* g);

const char* luau_bcg_type_info(LuauBcGraph* g, size_t* out_len);

// --- collection counts ---
size_t luau_bcg_block_count(LuauBcGraph* g);
size_t luau_bcg_inst_count(LuauBcGraph* g);
size_t luau_bcg_const_count(LuauBcGraph* g);
size_t luau_bcg_imm_count(LuauBcGraph* g);
size_t luau_bcg_phi_count(LuauBcGraph* g);
size_t luau_bcg_proj_count(LuauBcGraph* g);
size_t luau_bcg_proto_count(LuauBcGraph* g);
size_t luau_bcg_upvalue_type_count(LuauBcGraph* g);
size_t luau_bcg_local_type_count(LuauBcGraph* g);
size_t luau_bcg_upvalue_name_count(LuauBcGraph* g);
size_t luau_bcg_debug_local_count(LuauBcGraph* g);

unsigned int luau_bcg_proto_at(LuauBcGraph* g, size_t i);
// upvalueTypes[i] is a LuauBytecodeType (one byte enum), returned as int.
int luau_bcg_upvalue_type_at(LuauBcGraph* g, size_t i);
const char* luau_bcg_upvalue_name_at(LuauBcGraph* g, size_t i, size_t* out_len);

// localTypes[i] (Luau::Bytecode::TypedLocal).
int luau_bcg_local_type_at(LuauBcGraph* g, size_t i);
unsigned char luau_bcg_local_type_reg_at(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_local_type_startpc_at(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_local_type_endpc_at(LuauBcGraph* g, size_t i);

// debug locals[i] (Luau::Bytecode::DebugLocal).
const char* luau_bcg_debug_local_name_at(LuauBcGraph* g, size_t i, size_t* out_len);
unsigned char luau_bcg_debug_local_reg_at(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_debug_local_startpc_at(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_debug_local_endpc_at(LuauBcGraph* g, size_t i);

// --- instruction inspection (instructions[i]) ---
// op is a LuauOpcode (one byte) returned as int.
int luau_bcg_inst_op(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_inst_line(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_inst_last_use(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_inst_use_count(LuauBcGraph* g, size_t i);
// owning block: returns block index, or -1 if the block op is not a Block.
int luau_bcg_inst_block(LuauBcGraph* g, size_t i);
// operands: count, and kind/index per operand.
size_t luau_bcg_inst_operand_count(LuauBcGraph* g, size_t i);
int luau_bcg_inst_operand_kind(LuauBcGraph* g, size_t i, size_t k);
unsigned int luau_bcg_inst_operand_index(LuauBcGraph* g, size_t i, size_t k);

// --- block inspection (blocks[i]) ---
unsigned char luau_bcg_block_flags(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_block_use_count(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_block_startpc(LuauBcGraph* g, size_t i);
// ops is a std::list<BcOp>; exposed as count + kind/index at ordinal position.
size_t luau_bcg_block_op_count(LuauBcGraph* g, size_t i);
int luau_bcg_block_op_kind(LuauBcGraph* g, size_t i, size_t k);
unsigned int luau_bcg_block_op_index(LuauBcGraph* g, size_t i, size_t k);
// successor/predecessor edges.
size_t luau_bcg_block_successor_count(LuauBcGraph* g, size_t i);
size_t luau_bcg_block_predecessor_count(LuauBcGraph* g, size_t i);
int luau_bcg_block_successor_kind(LuauBcGraph* g, size_t i, size_t e);
int luau_bcg_block_successor_target(LuauBcGraph* g, size_t i, size_t e); // block idx or -1
int luau_bcg_block_predecessor_kind(LuauBcGraph* g, size_t i, size_t e);
int luau_bcg_block_predecessor_target(LuauBcGraph* g, size_t i, size_t e);

// --- immediate inspection (immediates[i], BcImm) ---
int luau_bcg_imm_kind(LuauBcGraph* g, size_t i);
int luau_bcg_imm_boolean(LuauBcGraph* g, size_t i);
int luau_bcg_imm_int(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_imm_import(LuauBcGraph* g, size_t i);

// --- constant inspection (constants[i], BcVmConst) ---
int luau_bcg_const_kind(LuauBcGraph* g, size_t i);
int luau_bcg_const_boolean(LuauBcGraph* g, size_t i);
double luau_bcg_const_number(LuauBcGraph* g, size_t i);
long long luau_bcg_const_integer(LuauBcGraph* g, size_t i);
float luau_bcg_const_vector(LuauBcGraph* g, size_t i, int component); // 0..3
const char* luau_bcg_const_string(LuauBcGraph* g, size_t i, size_t* out_len);
unsigned int luau_bcg_const_import(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_const_table(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_const_closure(LuauBcGraph* g, size_t i);

// --- phi / projection ---
size_t luau_bcg_phi_operand_count(LuauBcGraph* g, size_t i);
int luau_bcg_phi_operand_kind(LuauBcGraph* g, size_t i, size_t k);
unsigned int luau_bcg_phi_operand_index(LuauBcGraph* g, size_t i, size_t k);
int luau_bcg_proj_op_kind(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_proj_op_index(LuauBcGraph* g, size_t i);
unsigned int luau_bcg_proj_index(LuauBcGraph* g, size_t i);

LUAU_END_DECLS
