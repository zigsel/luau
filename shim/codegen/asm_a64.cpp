// extern "C" shim over Luau::CodeGen::A64::AssemblyBuilderA64 (the low-level
// arm64 JIT assembler). See asm_a64.h for the operand / register model.

#include "asm_a64.h"

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/AddressA64.h"
#include "Luau/ConditionA64.h"
#include "Luau/Label.h"
#include "Luau/RegisterA64.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

using namespace Luau::CodeGen;
using namespace Luau::CodeGen::A64;

// Operand handles each own a heap copy of the C++ value type.
struct LuauA64Reg {
    RegisterA64 value;
};

struct LuauA64Address {
    // AddressA64 has no default ctor (asserts in ctor), so wrap in a holder.
    AddressA64 value;
    LuauA64Address(AddressA64 v) : value(v) {}
};

// The builder owns a table mapping our exposed label ids to the builder's Label
// structs (the builder mutates Label::location during placement / finalize).
struct LuauAsmA64 {
    AssemblyBuilderA64 builder;
    std::unordered_map<uint32_t, Label> labels;
    uint32_t nextId = 1;

    LuauAsmA64(bool logText, unsigned int features)
        : builder(logText, features)
    {
    }
};

namespace {

RegisterA64 R(const LuauA64Reg* r) {
    return r->value;
}

AddressA64 A(const LuauA64Address* a) {
    return a->value;
}

ConditionA64 C(LuauA64Condition c) {
    return static_cast<ConditionA64>(c);
}

Label& labelRef(LuauAsmA64* a, uint32_t id) {
    auto it = a->labels.find(id);
    if (it == a->labels.end())
        it = a->labels.emplace(id, Label{}).first;
    return it->second;
}

} // namespace

// ---- register operand handles -------------------------------------------

extern "C" LuauA64Reg* luau_a64_reg(LuauA64Kind kind, int index) {
    try {
        return new LuauA64Reg{RegisterA64{static_cast<KindA64>(kind), static_cast<uint8_t>(index & 0x1f)}};
    } catch (...) {
        return nullptr;
    }
}

extern "C" LuauA64Reg* luau_a64_reg_sp(void) {
    try {
        return new LuauA64Reg{sp};
    } catch (...) {
        return nullptr;
    }
}

extern "C" LuauA64Reg* luau_a64_reg_noreg(void) {
    try {
        return new LuauA64Reg{noreg};
    } catch (...) {
        return nullptr;
    }
}

extern "C" LuauA64Reg* luau_a64_reg_cast(LuauA64Kind kind, const LuauA64Reg* reg) {
    try {
        return new LuauA64Reg{castReg(static_cast<KindA64>(kind), reg->value)};
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_a64_reg_free(LuauA64Reg* r) {
    delete r;
}

extern "C" LuauA64Kind luau_a64_reg_kind(const LuauA64Reg* r) {
    return static_cast<LuauA64Kind>(r->value.kind);
}

extern "C" int luau_a64_reg_index(const LuauA64Reg* r) {
    return r->value.index;
}

// ---- address operand handles --------------------------------------------

extern "C" LuauA64Address* luau_a64_address_imm(const LuauA64Reg* base, int offset, LuauA64AddressKind kind) {
    try {
        return new LuauA64Address(AddressA64(base->value, offset, static_cast<AddressKindA64>(kind)));
    } catch (...) {
        return nullptr;
    }
}

extern "C" LuauA64Address* luau_a64_address_reg(const LuauA64Reg* base, const LuauA64Reg* offset) {
    try {
        return new LuauA64Address(AddressA64(base->value, offset->value));
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_a64_address_free(LuauA64Address* a) {
    delete a;
}

// ---- lifetime ------------------------------------------------------------

extern "C" LuauAsmA64* luau_a64_asm_new(int log_text, unsigned int features) {
    try {
        return new LuauAsmA64(log_text != 0, features);
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_a64_asm_free(LuauAsmA64* a) {
    delete a;
}

// ---- labels --------------------------------------------------------------

extern "C" uint32_t luau_a64_asm_set_label_here(LuauAsmA64* a) {
    try {
        Label l = a->builder.setLabel();
        uint32_t id = a->nextId++;
        a->labels[id] = l;
        return id;
    } catch (...) {
        return 0;
    }
}

extern "C" uint32_t luau_a64_asm_make_label(LuauAsmA64* a) {
    uint32_t id = a->nextId++;
    a->labels[id] = Label{};
    return id;
}

extern "C" void luau_a64_asm_place_label(LuauAsmA64* a, uint32_t label_id) {
    try {
        a->builder.setLabel(labelRef(a, label_id));
    } catch (...) {
    }
}

extern "C" uint32_t luau_a64_asm_label_offset(LuauAsmA64* a, uint32_t label_id) {
    try {
        return a->builder.getLabelOffset(labelRef(a, label_id));
    } catch (...) {
        return 0;
    }
}

// ---- finalize / output ---------------------------------------------------

extern "C" int luau_a64_asm_finalize(LuauAsmA64* a) {
    try {
        return a->builder.finalize() ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

extern "C" uint32_t luau_a64_asm_code_size(LuauAsmA64* a) {
    return a->builder.getCodeSize() * uint32_t(sizeof(uint32_t));
}

extern "C" unsigned int luau_a64_asm_instruction_count(LuauAsmA64* a) {
    return a->builder.getInstructionCount();
}

extern "C" const uint8_t* luau_a64_asm_code_ptr(LuauAsmA64* a, size_t* out_len) {
    if (out_len)
        *out_len = a->builder.code.size() * sizeof(uint32_t);
    return reinterpret_cast<const uint8_t*>(a->builder.code.data());
}

extern "C" size_t luau_a64_asm_code_word_count(LuauAsmA64* a) {
    return a->builder.code.size();
}

extern "C" const uint8_t* luau_a64_asm_data_ptr(LuauAsmA64* a, size_t* out_len) {
    if (out_len)
        *out_len = a->builder.data.size();
    return a->builder.data.data();
}

extern "C" char* luau_a64_asm_get_text(LuauAsmA64* a) {
    try {
        const std::string& s = a->builder.text;
        char* out = static_cast<char*>(std::malloc(s.size() + 1));
        if (!out)
            return nullptr;
        std::memcpy(out, s.c_str(), s.size() + 1);
        return out;
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_a64_asm_log_append(LuauAsmA64* a, const char* text) {
    try {
        a->builder.logAppend("%s", text ? text : "");
    } catch (...) {
    }
}

// ---- static helpers ------------------------------------------------------

extern "C" int luau_a64_asm_is_mask_supported(uint32_t mask) {
    return AssemblyBuilderA64::isMaskSupported(mask) ? 1 : 0;
}

extern "C" int luau_a64_asm_is_fmov_supported_fp64(double value) {
    return AssemblyBuilderA64::isFmovSupportedFp64(value) ? 1 : 0;
}

extern "C" int luau_a64_asm_is_fmov_supported_fp32(float value) {
    return AssemblyBuilderA64::isFmovSupportedFp32(value) ? 1 : 0;
}

// Wrap every emit in try/catch; the builder may assert/throw on invalid input.
#define EMIT(body) \
    do { try { body; } catch (...) {} } while (0)

// ---- Moves ---------------------------------------------------------------

extern "C" void luau_a64_asm_mov(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.mov(R(dst), R(src)));
}
extern "C" void luau_a64_asm_mov_imm(LuauAsmA64* a, const LuauA64Reg* dst, int src) {
    EMIT(a->builder.mov(R(dst), src));
}
extern "C" void luau_a64_asm_movz(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift) {
    EMIT(a->builder.movz(R(dst), src, shift));
}
extern "C" void luau_a64_asm_movn(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift) {
    EMIT(a->builder.movn(R(dst), src, shift));
}
extern "C" void luau_a64_asm_movk(LuauAsmA64* a, const LuauA64Reg* dst, uint16_t src, int shift) {
    EMIT(a->builder.movk(R(dst), src, shift));
}

// ---- Arithmetic ----------------------------------------------------------

extern "C" void luau_a64_asm_add(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.add(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_add_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint16_t src2) {
    EMIT(a->builder.add(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_sub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.sub(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_sub_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint16_t src2) {
    EMIT(a->builder.sub(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_neg(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.neg(R(dst), R(src)));
}
extern "C" void luau_a64_asm_mul(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.mul(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_msub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, const LuauA64Reg* src3) {
    EMIT(a->builder.msub(R(dst), R(src1), R(src2), R(src3)));
}
extern "C" void luau_a64_asm_sdiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.sdiv(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_udiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.udiv(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_rem(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.rem(R(dst), R(src1), R(src2)));
}

// ---- Comparisons ---------------------------------------------------------

extern "C" void luau_a64_asm_cmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.cmp(R(src1), R(src2)));
}
extern "C" void luau_a64_asm_cmp_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint16_t src2) {
    EMIT(a->builder.cmp(R(src1), src2));
}
extern "C" void luau_a64_asm_ccmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond, uint8_t nzcv) {
    EMIT(a->builder.ccmp(R(src1), R(src2), C(cond), nzcv));
}
extern "C" void luau_a64_asm_ccmn(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond, uint8_t nzcv) {
    EMIT(a->builder.ccmn(R(src1), R(src2), C(cond), nzcv));
}
extern "C" void luau_a64_asm_ccmn_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint8_t src2, LuauA64Condition cond, uint8_t nzcv) {
    EMIT(a->builder.ccmn(R(src1), src2, C(cond), nzcv));
}
extern "C" void luau_a64_asm_cmn_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint16_t src2) {
    EMIT(a->builder.cmn(R(src1), src2));
}
extern "C" void luau_a64_asm_csel(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond) {
    EMIT(a->builder.csel(R(dst), R(src1), R(src2), C(cond)));
}
extern "C" void luau_a64_asm_cset(LuauAsmA64* a, const LuauA64Reg* dst, LuauA64Condition cond) {
    EMIT(a->builder.cset(R(dst), C(cond)));
}

// ---- Bitwise -------------------------------------------------------------

extern "C" void luau_a64_asm_and(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.and_(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_orr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.orr(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_eor(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.eor(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_bic(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.bic(R(dst), R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_tst(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, int shift) {
    EMIT(a->builder.tst(R(src1), R(src2), shift));
}
extern "C" void luau_a64_asm_mvn(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.mvn_(R(dst), R(src)));
}
extern "C" void luau_a64_asm_and_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2) {
    EMIT(a->builder.and_(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_orr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2) {
    EMIT(a->builder.orr(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_eor_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint32_t src2) {
    EMIT(a->builder.eor(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_tst_imm(LuauAsmA64* a, const LuauA64Reg* src1, uint32_t src2) {
    EMIT(a->builder.tst(R(src1), src2));
}

// ---- Shifts --------------------------------------------------------------

extern "C" void luau_a64_asm_lsl(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.lsl(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_lsr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.lsr(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_asr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.asr(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_ror(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.ror(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_clz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.clz(R(dst), R(src)));
}
extern "C" void luau_a64_asm_rbit(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.rbit(R(dst), R(src)));
}
extern "C" void luau_a64_asm_rev(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.rev(R(dst), R(src)));
}
extern "C" void luau_a64_asm_lsl_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2) {
    EMIT(a->builder.lsl(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_lsr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2) {
    EMIT(a->builder.lsr(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_asr_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2) {
    EMIT(a->builder.asr(R(dst), R(src1), src2));
}
extern "C" void luau_a64_asm_ror_imm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, uint8_t src2) {
    EMIT(a->builder.ror(R(dst), R(src1), src2));
}

// ---- Bitfields -----------------------------------------------------------

extern "C" void luau_a64_asm_ubfiz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w) {
    EMIT(a->builder.ubfiz(R(dst), R(src), f, w));
}
extern "C" void luau_a64_asm_ubfx(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w) {
    EMIT(a->builder.ubfx(R(dst), R(src), f, w));
}
extern "C" void luau_a64_asm_sbfiz(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w) {
    EMIT(a->builder.sbfiz(R(dst), R(src), f, w));
}
extern "C" void luau_a64_asm_sbfx(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t f, uint8_t w) {
    EMIT(a->builder.sbfx(R(dst), R(src), f, w));
}

// ---- Loads ---------------------------------------------------------------

extern "C" void luau_a64_asm_ldr(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldr(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldrb(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldrb(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldrh(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldrh(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldrsb(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldrsb(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldrsh(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldrsh(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldrsw(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Address* src) {
    EMIT(a->builder.ldrsw(R(dst), A(src)));
}
extern "C" void luau_a64_asm_ldp(LuauAsmA64* a, const LuauA64Reg* dst1, const LuauA64Reg* dst2, const LuauA64Address* src) {
    EMIT(a->builder.ldp(R(dst1), R(dst2), A(src)));
}

// ---- Stores --------------------------------------------------------------

extern "C" void luau_a64_asm_str(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst) {
    EMIT(a->builder.str(R(src), A(dst)));
}
extern "C" void luau_a64_asm_strb(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst) {
    EMIT(a->builder.strb(R(src), A(dst)));
}
extern "C" void luau_a64_asm_strh(LuauAsmA64* a, const LuauA64Reg* src, const LuauA64Address* dst) {
    EMIT(a->builder.strh(R(src), A(dst)));
}
extern "C" void luau_a64_asm_stp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2, const LuauA64Address* dst) {
    EMIT(a->builder.stp(R(src1), R(src2), A(dst)));
}

// ---- Control flow --------------------------------------------------------

extern "C" void luau_a64_asm_b(LuauAsmA64* a, uint32_t label_id) {
    EMIT(a->builder.b(labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_bl(LuauAsmA64* a, uint32_t label_id) {
    EMIT(a->builder.bl(labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_br(LuauAsmA64* a, const LuauA64Reg* src) {
    EMIT(a->builder.br(R(src)));
}
extern "C" void luau_a64_asm_blr(LuauAsmA64* a, const LuauA64Reg* src) {
    EMIT(a->builder.blr(R(src)));
}
extern "C" void luau_a64_asm_ret(LuauAsmA64* a) {
    EMIT(a->builder.ret());
}
extern "C" void luau_a64_asm_b_cond(LuauAsmA64* a, LuauA64Condition cond, uint32_t label_id) {
    EMIT(a->builder.b(C(cond), labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_cbz(LuauAsmA64* a, const LuauA64Reg* src, uint32_t label_id) {
    EMIT(a->builder.cbz(R(src), labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_cbnz(LuauAsmA64* a, const LuauA64Reg* src, uint32_t label_id) {
    EMIT(a->builder.cbnz(R(src), labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_tbz(LuauAsmA64* a, const LuauA64Reg* src, uint8_t bit, uint32_t label_id) {
    EMIT(a->builder.tbz(R(src), bit, labelRef(a, label_id)));
}
extern "C" void luau_a64_asm_tbnz(LuauAsmA64* a, const LuauA64Reg* src, uint8_t bit, uint32_t label_id) {
    EMIT(a->builder.tbnz(R(src), bit, labelRef(a, label_id)));
}

// ---- Address of embedded data / code ------------------------------------

extern "C" void luau_a64_asm_adr_data(LuauAsmA64* a, const LuauA64Reg* dst, const void* ptr, size_t size) {
    EMIT(a->builder.adr(R(dst), ptr, size));
}
extern "C" void luau_a64_asm_adr_u64(LuauAsmA64* a, const LuauA64Reg* dst, uint64_t value) {
    EMIT(a->builder.adr(R(dst), value));
}
extern "C" void luau_a64_asm_adr_f32(LuauAsmA64* a, const LuauA64Reg* dst, float value) {
    EMIT(a->builder.adr(R(dst), value));
}
extern "C" void luau_a64_asm_adr_f64(LuauAsmA64* a, const LuauA64Reg* dst, double value) {
    EMIT(a->builder.adr(R(dst), value));
}
extern "C" void luau_a64_asm_adr_label(LuauAsmA64* a, const LuauA64Reg* dst, uint32_t label_id) {
    EMIT(a->builder.adr(R(dst), labelRef(a, label_id)));
}

// ---- Floating-point / SIMD moves ----------------------------------------

extern "C" void luau_a64_asm_fmov(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fmov(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fmov_f64(LuauAsmA64* a, const LuauA64Reg* dst, double src) {
    EMIT(a->builder.fmov(R(dst), src));
}
extern "C" void luau_a64_asm_fmov_f32(LuauAsmA64* a, const LuauA64Reg* dst, float src) {
    EMIT(a->builder.fmov(R(dst), src));
}

// ---- Floating-point / SIMD math -----------------------------------------

extern "C" void luau_a64_asm_fabs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fabs(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fadd(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fadd(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_fdiv(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fdiv(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_fmul(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fmul(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_fneg(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fneg(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fsqrt(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fsqrt(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fsub(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fsub(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_faddp(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.faddp(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fmla(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fmla(R(dst), R(src1), R(src2)));
}

// ---- Vector component manipulation --------------------------------------

extern "C" void luau_a64_asm_ins_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index) {
    EMIT(a->builder.ins_4s(R(dst), R(src), index));
}
extern "C" void luau_a64_asm_ins_4s_idx(LuauAsmA64* a, const LuauA64Reg* dst, uint8_t dstIndex, const LuauA64Reg* src, uint8_t srcIndex) {
    EMIT(a->builder.ins_4s(R(dst), dstIndex, R(src), srcIndex));
}
extern "C" void luau_a64_asm_dup_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index) {
    EMIT(a->builder.dup_4s(R(dst), R(src), index));
}
extern "C" void luau_a64_asm_umov_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, uint8_t index) {
    EMIT(a->builder.umov_4s(R(dst), R(src), index));
}
extern "C" void luau_a64_asm_fcmeq_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fcmeq_4s(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_fcmgt_4s(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fcmgt_4s(R(dst), R(src1), R(src2)));
}
extern "C" void luau_a64_asm_bit(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, const LuauA64Reg* mask) {
    EMIT(a->builder.bit(R(dst), R(src), R(mask)));
}
extern "C" void luau_a64_asm_bif(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src, const LuauA64Reg* mask) {
    EMIT(a->builder.bif(R(dst), R(src), R(mask)));
}

// ---- FP rounding and conversions ----------------------------------------

extern "C" void luau_a64_asm_frinta(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.frinta(R(dst), R(src)));
}
extern "C" void luau_a64_asm_frintm(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.frintm(R(dst), R(src)));
}
extern "C" void luau_a64_asm_frintp(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.frintp(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fcvt(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fcvt(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fcvtzs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fcvtzs(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fcvtzu(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fcvtzu(R(dst), R(src)));
}
extern "C" void luau_a64_asm_scvtf(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.scvtf(R(dst), R(src)));
}
extern "C" void luau_a64_asm_ucvtf(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.ucvtf(R(dst), R(src)));
}
extern "C" void luau_a64_asm_fjcvtzs(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src) {
    EMIT(a->builder.fjcvtzs(R(dst), R(src)));
}

// ---- FP comparisons ------------------------------------------------------

extern "C" void luau_a64_asm_fcmp(LuauAsmA64* a, const LuauA64Reg* src1, const LuauA64Reg* src2) {
    EMIT(a->builder.fcmp(R(src1), R(src2)));
}
extern "C" void luau_a64_asm_fcmpz(LuauAsmA64* a, const LuauA64Reg* src) {
    EMIT(a->builder.fcmpz(R(src)));
}
extern "C" void luau_a64_asm_fcsel(LuauAsmA64* a, const LuauA64Reg* dst, const LuauA64Reg* src1, const LuauA64Reg* src2, LuauA64Condition cond) {
    EMIT(a->builder.fcsel(R(dst), R(src1), R(src2), C(cond)));
}

// ---- Misc ----------------------------------------------------------------

extern "C" void luau_a64_asm_udf(LuauAsmA64* a) {
    EMIT(a->builder.udf());
}
extern "C" void luau_a64_asm_nop(LuauAsmA64* a, uint32_t bytes) {
    EMIT(a->builder.nop(bytes));
}
