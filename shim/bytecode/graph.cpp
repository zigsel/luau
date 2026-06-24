// extern "C" shim over Luau::Bytecode::CompTimeBcFunction (BytecodeGraph.h).

#include "graph.h"

#include "Luau/BytecodeGraph.h"

#include <iterator>
#include <string>
#include <vector>

using namespace Luau;
using namespace Luau::Bytecode;

struct LuauBcGraph {
    CompTimeBcFunction func;
    // String views in `func` borrow these; keep them alive for the graph's life.
    std::vector<std::string> stringStorage;
    std::vector<std::string_view> strings;
    // Scratch buffer backing the const char* from to_function_bytecode.
    std::string scratch;
};

// Resolve a BcOp that is expected to reference a block to its index, else -1.
static int blockIndexOf(BcOp op) {
    return op.kind == BcOpKind::Block ? static_cast<int>(op.index) : -1;
}

extern "C" LuauBcGraph* luau_bcg_from_function_bytecode(const char* bytecode, size_t bytecode_len,
                                                        const char* const* string_ptrs,
                                                        const size_t* string_lens,
                                                        size_t string_count) {
    try {
        LuauBcGraph* g = new LuauBcGraph();
        g->stringStorage.reserve(string_count);
        g->strings.reserve(string_count);
        for (size_t i = 0; i < string_count; ++i)
            g->stringStorage.emplace_back(string_ptrs[i], string_lens[i]);
        for (size_t i = 0; i < string_count; ++i)
            g->strings.emplace_back(g->stringStorage[i].data(), g->stringStorage[i].size());

        std::optional<CompTimeBcFunction> parsed =
            fromFunctionBytecode(std::string(bytecode, bytecode_len), g->strings);
        if (!parsed) {
            delete g;
            return nullptr;
        }
        g->func = std::move(*parsed);
        return g;
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_bcg_free(LuauBcGraph* g) {
    delete g;
}

extern "C" const char* luau_bcg_to_function_bytecode(LuauBcGraph* g, size_t* out_len) {
    try {
        g->scratch = toFunctionBytecode(g->func);
        if (out_len) *out_len = g->scratch.size();
        return g->scratch.data();
    } catch (...) {
        if (out_len) *out_len = 0;
        return nullptr;
    }
}

// --- function-level scalars ---
extern "C" unsigned char luau_bcg_max_stack_size(LuauBcGraph* g) { return g->func.maxstacksize; }
extern "C" unsigned char luau_bcg_num_params(LuauBcGraph* g) { return g->func.numparams; }
extern "C" unsigned char luau_bcg_num_upvalues(LuauBcGraph* g) { return g->func.nups; }
extern "C" int luau_bcg_is_vararg(LuauBcGraph* g) { return g->func.is_vararg ? 1 : 0; }
extern "C" unsigned char luau_bcg_flags(LuauBcGraph* g) { return g->func.flags; }
extern "C" unsigned int luau_bcg_line_defined(LuauBcGraph* g) { return g->func.linedefined; }
extern "C" const char* luau_bcg_debug_name(LuauBcGraph* g, size_t* out_len) {
    if (out_len) *out_len = g->func.debugname.size();
    return g->func.debugname.data();
}
extern "C" int luau_bcg_entry_block(LuauBcGraph* g) { return blockIndexOf(g->func.entryBlock); }
extern "C" int luau_bcg_exit_block(LuauBcGraph* g) { return blockIndexOf(g->func.exitBlock); }
extern "C" const char* luau_bcg_type_info(LuauBcGraph* g, size_t* out_len) {
    if (out_len) *out_len = g->func.typeInfo.size();
    return g->func.typeInfo.data();
}

// --- collection counts ---
extern "C" size_t luau_bcg_block_count(LuauBcGraph* g) { return g->func.blocks.size(); }
extern "C" size_t luau_bcg_inst_count(LuauBcGraph* g) { return g->func.instructions.size(); }
extern "C" size_t luau_bcg_const_count(LuauBcGraph* g) { return g->func.constants.size(); }
extern "C" size_t luau_bcg_imm_count(LuauBcGraph* g) { return g->func.immediates.size(); }
extern "C" size_t luau_bcg_phi_count(LuauBcGraph* g) { return g->func.phis.size(); }
extern "C" size_t luau_bcg_proj_count(LuauBcGraph* g) { return g->func.projections.size(); }
extern "C" size_t luau_bcg_proto_count(LuauBcGraph* g) { return g->func.protos.size(); }
extern "C" size_t luau_bcg_upvalue_type_count(LuauBcGraph* g) { return g->func.upvalueTypes.size(); }
extern "C" size_t luau_bcg_local_type_count(LuauBcGraph* g) { return g->func.localTypes.size(); }
extern "C" size_t luau_bcg_upvalue_name_count(LuauBcGraph* g) { return g->func.upvalueNames.size(); }
extern "C" size_t luau_bcg_debug_local_count(LuauBcGraph* g) { return g->func.locals.size(); }

extern "C" unsigned int luau_bcg_proto_at(LuauBcGraph* g, size_t i) { return g->func.protos[i]; }
extern "C" int luau_bcg_upvalue_type_at(LuauBcGraph* g, size_t i) {
    return static_cast<int>(g->func.upvalueTypes[i]);
}
extern "C" const char* luau_bcg_upvalue_name_at(LuauBcGraph* g, size_t i, size_t* out_len) {
    if (out_len) *out_len = g->func.upvalueNames[i].size();
    return g->func.upvalueNames[i].data();
}

extern "C" int luau_bcg_local_type_at(LuauBcGraph* g, size_t i) {
    return static_cast<int>(g->func.localTypes[i].type);
}
extern "C" unsigned char luau_bcg_local_type_reg_at(LuauBcGraph* g, size_t i) { return g->func.localTypes[i].reg; }
extern "C" unsigned int luau_bcg_local_type_startpc_at(LuauBcGraph* g, size_t i) { return g->func.localTypes[i].startpc; }
extern "C" unsigned int luau_bcg_local_type_endpc_at(LuauBcGraph* g, size_t i) { return g->func.localTypes[i].endpc; }

extern "C" const char* luau_bcg_debug_local_name_at(LuauBcGraph* g, size_t i, size_t* out_len) {
    if (out_len) *out_len = g->func.locals[i].varname.size();
    return g->func.locals[i].varname.data();
}
extern "C" unsigned char luau_bcg_debug_local_reg_at(LuauBcGraph* g, size_t i) { return g->func.locals[i].reg; }
extern "C" unsigned int luau_bcg_debug_local_startpc_at(LuauBcGraph* g, size_t i) { return g->func.locals[i].startpc; }
extern "C" unsigned int luau_bcg_debug_local_endpc_at(LuauBcGraph* g, size_t i) { return g->func.locals[i].endpc; }

// --- instruction inspection ---
extern "C" int luau_bcg_inst_op(LuauBcGraph* g, size_t i) { return static_cast<int>(g->func.instructions[i].op); }
extern "C" unsigned int luau_bcg_inst_line(LuauBcGraph* g, size_t i) { return g->func.instructions[i].line; }
extern "C" unsigned int luau_bcg_inst_last_use(LuauBcGraph* g, size_t i) { return g->func.instructions[i].lastUse; }
extern "C" unsigned int luau_bcg_inst_use_count(LuauBcGraph* g, size_t i) { return g->func.instructions[i].useCount; }
extern "C" int luau_bcg_inst_block(LuauBcGraph* g, size_t i) { return blockIndexOf(g->func.instructions[i].block); }
extern "C" size_t luau_bcg_inst_operand_count(LuauBcGraph* g, size_t i) { return g->func.instructions[i].ops.size(); }
extern "C" int luau_bcg_inst_operand_kind(LuauBcGraph* g, size_t i, size_t k) {
    return static_cast<int>(g->func.instructions[i].ops[k].kind);
}
extern "C" unsigned int luau_bcg_inst_operand_index(LuauBcGraph* g, size_t i, size_t k) {
    return g->func.instructions[i].ops[k].index;
}

// --- block inspection ---
extern "C" unsigned char luau_bcg_block_flags(LuauBcGraph* g, size_t i) { return g->func.blocks[i].flags; }
extern "C" unsigned int luau_bcg_block_use_count(LuauBcGraph* g, size_t i) { return g->func.blocks[i].useCount; }
extern "C" unsigned int luau_bcg_block_startpc(LuauBcGraph* g, size_t i) { return g->func.blocks[i].startpc; }
extern "C" size_t luau_bcg_block_op_count(LuauBcGraph* g, size_t i) { return g->func.blocks[i].ops.size(); }
extern "C" int luau_bcg_block_op_kind(LuauBcGraph* g, size_t i, size_t k) {
    const std::list<BcOp>& ops = g->func.blocks[i].ops;
    auto it = ops.begin();
    std::advance(it, k);
    return static_cast<int>(it->kind);
}
extern "C" unsigned int luau_bcg_block_op_index(LuauBcGraph* g, size_t i, size_t k) {
    const std::list<BcOp>& ops = g->func.blocks[i].ops;
    auto it = ops.begin();
    std::advance(it, k);
    return it->index;
}
extern "C" size_t luau_bcg_block_successor_count(LuauBcGraph* g, size_t i) { return g->func.blocks[i].successors.size(); }
extern "C" size_t luau_bcg_block_predecessor_count(LuauBcGraph* g, size_t i) { return g->func.blocks[i].predecessors.size(); }
extern "C" int luau_bcg_block_successor_kind(LuauBcGraph* g, size_t i, size_t e) {
    return static_cast<int>(g->func.blocks[i].successors[e].kind);
}
extern "C" int luau_bcg_block_successor_target(LuauBcGraph* g, size_t i, size_t e) {
    return blockIndexOf(g->func.blocks[i].successors[e].target);
}
extern "C" int luau_bcg_block_predecessor_kind(LuauBcGraph* g, size_t i, size_t e) {
    return static_cast<int>(g->func.blocks[i].predecessors[e].kind);
}
extern "C" int luau_bcg_block_predecessor_target(LuauBcGraph* g, size_t i, size_t e) {
    return blockIndexOf(g->func.blocks[i].predecessors[e].target);
}

// --- immediate inspection ---
extern "C" int luau_bcg_imm_kind(LuauBcGraph* g, size_t i) { return static_cast<int>(g->func.immediates[i].kind); }
extern "C" int luau_bcg_imm_boolean(LuauBcGraph* g, size_t i) { return g->func.immediates[i].valueBoolean ? 1 : 0; }
extern "C" int luau_bcg_imm_int(LuauBcGraph* g, size_t i) { return g->func.immediates[i].valueInt; }
extern "C" unsigned int luau_bcg_imm_import(LuauBcGraph* g, size_t i) { return g->func.immediates[i].valueImport; }

// --- constant inspection ---
extern "C" int luau_bcg_const_kind(LuauBcGraph* g, size_t i) { return static_cast<int>(g->func.constants[i].kind); }
extern "C" int luau_bcg_const_boolean(LuauBcGraph* g, size_t i) { return g->func.constants[i].valueBoolean ? 1 : 0; }
extern "C" double luau_bcg_const_number(LuauBcGraph* g, size_t i) { return g->func.constants[i].valueNumber; }
extern "C" long long luau_bcg_const_integer(LuauBcGraph* g, size_t i) {
    return static_cast<long long>(g->func.constants[i].valueInteger);
}
extern "C" float luau_bcg_const_vector(LuauBcGraph* g, size_t i, int component) {
    if (component < 0 || component > 3) return 0.0f;
    return g->func.constants[i].valueVector[component];
}
extern "C" const char* luau_bcg_const_string(LuauBcGraph* g, size_t i, size_t* out_len) {
    const std::string_view& sv = g->func.constants[i].valueString;
    if (out_len) *out_len = sv.size();
    return sv.data();
}
extern "C" unsigned int luau_bcg_const_import(LuauBcGraph* g, size_t i) { return g->func.constants[i].valueImport; }
extern "C" unsigned int luau_bcg_const_table(LuauBcGraph* g, size_t i) { return g->func.constants[i].valueTable; }
extern "C" unsigned int luau_bcg_const_closure(LuauBcGraph* g, size_t i) { return g->func.constants[i].valueClosure; }

// --- phi / projection ---
extern "C" size_t luau_bcg_phi_operand_count(LuauBcGraph* g, size_t i) { return g->func.phis[i].ops.size(); }
extern "C" int luau_bcg_phi_operand_kind(LuauBcGraph* g, size_t i, size_t k) {
    return static_cast<int>(g->func.phis[i].ops[k].kind);
}
extern "C" unsigned int luau_bcg_phi_operand_index(LuauBcGraph* g, size_t i, size_t k) {
    return g->func.phis[i].ops[k].index;
}
extern "C" int luau_bcg_proj_op_kind(LuauBcGraph* g, size_t i) { return static_cast<int>(g->func.projections[i].op.kind); }
extern "C" unsigned int luau_bcg_proj_op_index(LuauBcGraph* g, size_t i) { return g->func.projections[i].op.index; }
extern "C" unsigned int luau_bcg_proj_index(LuauBcGraph* g, size_t i) { return g->func.projections[i].index; }
