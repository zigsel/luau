// extern "C" shim over Luau::CodeGen::IrBuilder / IrData (the JIT IR).
//
// VM-internal boundary: IrBuilder::buildFunctionIr / translateInst /
// rebuildBytecodeBasicBlocks / handleFastcallFallback all take a `Proto*` (the
// VM's compiled-function representation, defined in the VM's private lobject.h).
// We cannot synthesize a Proto without VM internals, so those high-level
// lowering entry points are intentionally not bound. The factory + inspection
// surface below operates purely on the IrFunction the builder owns and needs no
// Proto.

#include "ir.h"

#include "Luau/CodeGenOptions.h" // HostIrHooks (default-constructible)
#include "Luau/IrBuilder.h"
#include "Luau/IrData.h"

#include <cstring>

using namespace Luau::CodeGen;

// The handle owns the default host hooks (all nullptr) alongside the builder,
// since IrBuilder stores a reference to them.
struct LuauIrBuilder {
    HostIrHooks hooks;
    IrBuilder builder;

    LuauIrBuilder()
        : builder(hooks)
    {
    }
};

namespace {

// IrOp is a bitfield { kind:4, index:28 }; pack/unpack via memcpy to match the
// exact in-memory layout the Zig side expects.
LuauIrOp packOp(IrOp op) {
    uint32_t v;
    static_assert(sizeof(IrOp) == sizeof(uint32_t), "IrOp must be 4 bytes");
    std::memcpy(&v, &op, sizeof(v));
    return v;
}

IrOp unpackOp(LuauIrOp v) {
    IrOp op;
    std::memcpy(&op, &v, sizeof(op));
    return op;
}

} // namespace

extern "C" LuauIrOpKind luau_irop_kind(LuauIrOp op) {
    return static_cast<LuauIrOpKind>(unpackOp(op).kind);
}

extern "C" uint32_t luau_irop_index(LuauIrOp op) {
    return unpackOp(op).index;
}

// ---- lifetime ------------------------------------------------------------

extern "C" LuauIrBuilder* luau_irbuilder_new(void) {
    try {
        return new LuauIrBuilder();
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_irbuilder_free(LuauIrBuilder* b) {
    delete b;
}

// ---- factories -----------------------------------------------------------

#define FACTORY0(name, call)                                       \
    extern "C" LuauIrOp name(LuauIrBuilder* b) {                   \
        try {                                                      \
            return packOp(b->builder.call);                        \
        } catch (...) {                                            \
            return 0;                                              \
        }                                                          \
    }

FACTORY0(luau_irbuilder_undef, undef())

#undef FACTORY0

extern "C" LuauIrOp luau_irbuilder_const_int(LuauIrBuilder* b, int value) {
    try { return packOp(b->builder.constInt(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_const_int64(LuauIrBuilder* b, int64_t value) {
    try { return packOp(b->builder.constInt64(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_const_uint(LuauIrBuilder* b, unsigned int value) {
    try { return packOp(b->builder.constUint(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_const_double(LuauIrBuilder* b, double value) {
    try { return packOp(b->builder.constDouble(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_const_tag(LuauIrBuilder* b, uint8_t value) {
    try { return packOp(b->builder.constTag(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_const_import(LuauIrBuilder* b, unsigned int value) {
    try { return packOp(b->builder.constImport(value)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_cond(LuauIrBuilder* b, LuauIrCondition cond) {
    try { return packOp(b->builder.cond(static_cast<IrCondition>(cond))); } catch (...) { return 0; }
}

extern "C" LuauIrOp luau_irbuilder_vmreg(LuauIrBuilder* b, uint8_t index) {
    try { return packOp(b->builder.vmReg(index)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_vmconst(LuauIrBuilder* b, uint32_t index) {
    try { return packOp(b->builder.vmConst(index)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_vmupvalue(LuauIrBuilder* b, uint8_t index) {
    try { return packOp(b->builder.vmUpvalue(index)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_vmexit(LuauIrBuilder* b, uint32_t pcpos) {
    try { return packOp(b->builder.vmExit(pcpos)); } catch (...) { return 0; }
}

extern "C" LuauIrOp luau_irbuilder_const_any(LuauIrBuilder* b, LuauIrConstKind kind, uint64_t bits, uint64_t as_common_key) {
    try {
        IrConst constant{};
        constant.kind = static_cast<IrConstKind>(kind);
        // The union members alias the same storage; populate via the widest one
        // (valueInt64) so the requested bit pattern lands intact regardless of
        // which typed accessor reads it back.
        constant.valueInt64 = static_cast<int64_t>(bits);
        return packOp(b->builder.constAny(constant, as_common_key));
    } catch (...) {
        return 0;
    }
}

extern "C" LuauIrOp luau_irbuilder_block(LuauIrBuilder* b, LuauIrBlockKind kind) {
    try { return packOp(b->builder.block(static_cast<IrBlockKind>(kind))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_block_at_inst(LuauIrBuilder* b, uint32_t index) {
    try { return packOp(b->builder.blockAtInst(index)); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_fallback_block(LuauIrBuilder* b, uint32_t pcpos) {
    try { return packOp(b->builder.fallbackBlock(pcpos)); } catch (...) { return 0; }
}
extern "C" void luau_irbuilder_begin_block(LuauIrBuilder* b, LuauIrOp block) {
    try { b->builder.beginBlock(unpackOp(block)); } catch (...) {}
}
extern "C" int luau_irbuilder_is_internal_block(LuauIrBuilder* b, LuauIrOp block) {
    try { return b->builder.isInternalBlock(unpackOp(block)) ? 1 : 0; } catch (...) { return 0; }
}

extern "C" void luau_irbuilder_load_and_check_tag(LuauIrBuilder* b, LuauIrOp loc, uint8_t tag, LuauIrOp fallback) {
    try { b->builder.loadAndCheckTag(unpackOp(loc), tag, unpackOp(fallback)); } catch (...) {}
}
extern "C" void luau_irbuilder_check_safe_env(LuauIrBuilder* b, int pcpos) {
    try { b->builder.checkSafeEnv(pcpos); } catch (...) {}
}

extern "C" LuauIrOp luau_irbuilder_inst0(LuauIrBuilder* b, uint8_t cmd) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst1(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst2(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst3(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c), unpackOp(d))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst4(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c), unpackOp(d), unpackOp(e))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst5(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c), unpackOp(d), unpackOp(e), unpackOp(f))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst6(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f, LuauIrOp g) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c), unpackOp(d), unpackOp(e), unpackOp(f), unpackOp(g))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst7(LuauIrBuilder* b, uint8_t cmd, LuauIrOp a, LuauIrOp c, LuauIrOp d, LuauIrOp e, LuauIrOp f, LuauIrOp g, LuauIrOp h) {
    try { return packOp(b->builder.inst(static_cast<IrCmd>(cmd), unpackOp(a), unpackOp(c), unpackOp(d), unpackOp(e), unpackOp(f), unpackOp(g), unpackOp(h))); } catch (...) { return 0; }
}
extern "C" LuauIrOp luau_irbuilder_inst_n(LuauIrBuilder* b, uint8_t cmd, const LuauIrOp* ops, size_t count) {
    try {
        IrOps list;
        for (size_t i = 0; i < count; i++)
            list.push_back(unpackOp(ops[i]));
        return packOp(b->builder.inst(static_cast<IrCmd>(cmd), list));
    } catch (...) {
        return 0;
    }
}

// ---- inspection ----------------------------------------------------------

extern "C" size_t luau_irbuilder_const_count(LuauIrBuilder* b) {
    return b->builder.function.constants.size();
}
extern "C" size_t luau_irbuilder_inst_count(LuauIrBuilder* b) {
    return b->builder.function.instructions.size();
}
extern "C" size_t luau_irbuilder_block_count(LuauIrBuilder* b) {
    return b->builder.function.blocks.size();
}

extern "C" int luau_irbuilder_const_kind(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    if (idx >= c.size())
        return -1;
    return static_cast<int>(c[idx].kind);
}
extern "C" int luau_irbuilder_const_get_int(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? c[idx].valueInt : 0;
}
extern "C" int64_t luau_irbuilder_const_get_int64(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? c[idx].valueInt64 : 0;
}
extern "C" unsigned int luau_irbuilder_const_get_uint(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? c[idx].valueUint : 0;
}
extern "C" double luau_irbuilder_const_get_double(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? c[idx].valueDouble : 0.0;
}
extern "C" uint8_t luau_irbuilder_const_get_tag(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? c[idx].valueTag : 0;
}

extern "C" uint64_t luau_irbuilder_const_get_bits(LuauIrBuilder* b, size_t idx) {
    const auto& c = b->builder.function.constants;
    return idx < c.size() ? static_cast<uint64_t>(c[idx].valueInt64) : 0;
}

extern "C" int luau_irbuilder_inst_cmd(LuauIrBuilder* b, size_t idx) {
    const auto& ins = b->builder.function.instructions;
    if (idx >= ins.size())
        return -1;
    return static_cast<int>(ins[idx].cmd);
}
extern "C" size_t luau_irbuilder_inst_op_count(LuauIrBuilder* b, size_t idx) {
    const auto& ins = b->builder.function.instructions;
    if (idx >= ins.size())
        return 0;
    return ins[idx].ops.size();
}
extern "C" LuauIrOp luau_irbuilder_inst_op(LuauIrBuilder* b, size_t idx, size_t op_idx) {
    const auto& ins = b->builder.function.instructions;
    if (idx >= ins.size())
        return 0;
    const auto& ops = ins[idx].ops;
    if (op_idx >= ops.size())
        return 0;
    return packOp(ops[op_idx]);
}
extern "C" uint32_t luau_irbuilder_inst_last_use(LuauIrBuilder* b, size_t idx) {
    const auto& ins = b->builder.function.instructions;
    return idx < ins.size() ? ins[idx].lastUse : 0;
}
extern "C" uint32_t luau_irbuilder_inst_use_count(LuauIrBuilder* b, size_t idx) {
    const auto& ins = b->builder.function.instructions;
    return idx < ins.size() ? ins[idx].useCount : 0;
}

// ---- block inspection ----------------------------------------------------

extern "C" int luau_irbuilder_block_kind(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    if (idx >= bl.size())
        return -1;
    return static_cast<int>(bl[idx].kind);
}
extern "C" uint8_t luau_irbuilder_block_flags(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    return idx < bl.size() ? bl[idx].flags : 0;
}
extern "C" uint16_t luau_irbuilder_block_use_count(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    return idx < bl.size() ? bl[idx].useCount : 0;
}
extern "C" uint32_t luau_irbuilder_block_start(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    return idx < bl.size() ? bl[idx].start : ~0u;
}
extern "C" uint32_t luau_irbuilder_block_finish(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    return idx < bl.size() ? bl[idx].finish : ~0u;
}
extern "C" uint32_t luau_irbuilder_block_startpc(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    return idx < bl.size() ? bl[idx].startpc : ~0u;
}
extern "C" size_t luau_irbuilder_block_inst_count(LuauIrBuilder* b, size_t idx) {
    const auto& bl = b->builder.function.blocks;
    if (idx >= bl.size())
        return 0;
    const IrBlock& block = bl[idx];
    if (block.start == ~0u || block.finish == ~0u || block.finish < block.start)
        return 0;
    return size_t(block.finish - block.start) + 1;
}
extern "C" uint32_t luau_irbuilder_block_inst_at(LuauIrBuilder* b, size_t idx, size_t n) {
    const auto& bl = b->builder.function.blocks;
    if (idx >= bl.size())
        return ~0u;
    const IrBlock& block = bl[idx];
    if (block.start == ~0u || block.finish == ~0u || block.finish < block.start)
        return ~0u;
    if (n > size_t(block.finish - block.start))
        return ~0u;
    return block.start + uint32_t(n);
}

// ---- function / builder state --------------------------------------------

extern "C" uint32_t luau_irbuilder_entry_block(LuauIrBuilder* b) {
    return b->builder.function.entryBlock;
}
extern "C" int luau_irbuilder_in_terminated_block(LuauIrBuilder* b) {
    return b->builder.inTerminatedBlock ? 1 : 0;
}
extern "C" uint32_t luau_irbuilder_active_block_idx(LuauIrBuilder* b) {
    return b->builder.activeBlockIdx;
}
