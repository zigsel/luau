// extern "C" shim over Luau::CodeGen::X64::AssemblyBuilderX64 — the FULL x64
// assembler, with every public instruction/method bound. Operands (RegisterX64,
// OperandX64) and Label are exposed as opaque, heap-owned value handles.

#include "asm_x64.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/ConditionX64.h"
#include "Luau/Label.h"
#include "Luau/OperandX64.h"
#include "Luau/RegisterX64.h"

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

using namespace Luau::CodeGen;
using namespace Luau::CodeGen::X64;

// ---- handle structs ------------------------------------------------------

struct LuauX64Reg {
    RegisterX64 value;
};

struct LuauX64Operand {
    OperandX64 value;
};

struct LuauX64Label {
    Label value;
};

struct LuauX64Asm {
    AssemblyBuilderX64 builder;

    LuauX64Asm(bool logText, ABIX64 abi, unsigned int features)
        : builder(logText, abi, features)
    {
    }
    LuauX64Asm(bool logText, unsigned int features)
        : builder(logText, features)
    {
    }
};

namespace {

LuauX64Reg* newReg(RegisterX64 r) {
    return new (std::nothrow) LuauX64Reg{r};
}

LuauX64Operand* newOp(OperandX64 o) {
    return new (std::nothrow) LuauX64Operand{o};
}

RegisterX64 regOf(const LuauX64Reg* r) {
    return r ? r->value : noreg;
}

const OperandX64& opOf(const LuauX64Operand* o) {
    return o->value;
}

ConditionX64 toCond(LuauX64Cond c) {
    return static_cast<ConditionX64>(c);
}

RoundingModeX64 toRounding(LuauX64RoundingMode m) {
    return static_cast<RoundingModeX64>(m);
}

} // namespace

// ---- register handles ----------------------------------------------------

extern "C" LuauX64Reg* luau_x64_reg(LuauX64Size size, uint8_t index) {
    return newReg(RegisterX64{static_cast<SizeX64>(size), static_cast<uint8_t>(index & 0x1f)});
}

extern "C" LuauX64Reg* luau_x64_reg_clone(const LuauX64Reg* r) {
    return r ? newReg(r->value) : nullptr;
}

extern "C" void luau_x64_reg_free(LuauX64Reg* r) {
    delete r;
}

extern "C" LuauX64Size luau_x64_reg_size(const LuauX64Reg* r) {
    return static_cast<LuauX64Size>(r->value.size);
}

extern "C" uint8_t luau_x64_reg_index(const LuauX64Reg* r) {
    return r->value.index;
}

extern "C" LuauX64Reg* luau_x64_reg_byte(const LuauX64Reg* r) {
    return newReg(byteReg(r->value));
}
extern "C" LuauX64Reg* luau_x64_reg_word(const LuauX64Reg* r) {
    return newReg(wordReg(r->value));
}
extern "C" LuauX64Reg* luau_x64_reg_dword(const LuauX64Reg* r) {
    return newReg(dwordReg(r->value));
}
extern "C" LuauX64Reg* luau_x64_reg_qword(const LuauX64Reg* r) {
    return newReg(qwordReg(r->value));
}

#define NAMED_REG(fn, reg)                                  \
    extern "C" LuauX64Reg* fn(void) { return newReg(reg); }

NAMED_REG(luau_x64_reg_noreg, noreg)
NAMED_REG(luau_x64_reg_rip, rip)

NAMED_REG(luau_x64_reg_al, al)
NAMED_REG(luau_x64_reg_cl, cl)
NAMED_REG(luau_x64_reg_dl, dl)
NAMED_REG(luau_x64_reg_bl, bl)
NAMED_REG(luau_x64_reg_spl, spl)
NAMED_REG(luau_x64_reg_bpl, bpl)
NAMED_REG(luau_x64_reg_sil, sil)
NAMED_REG(luau_x64_reg_dil, dil)
NAMED_REG(luau_x64_reg_r8b, r8b)
NAMED_REG(luau_x64_reg_r9b, r9b)
NAMED_REG(luau_x64_reg_r10b, r10b)
NAMED_REG(luau_x64_reg_r11b, r11b)
NAMED_REG(luau_x64_reg_r12b, r12b)
NAMED_REG(luau_x64_reg_r13b, r13b)
NAMED_REG(luau_x64_reg_r14b, r14b)
NAMED_REG(luau_x64_reg_r15b, r15b)

NAMED_REG(luau_x64_reg_eax, eax)
NAMED_REG(luau_x64_reg_ecx, ecx)
NAMED_REG(luau_x64_reg_edx, edx)
NAMED_REG(luau_x64_reg_ebx, ebx)
NAMED_REG(luau_x64_reg_esp, esp)
NAMED_REG(luau_x64_reg_ebp, ebp)
NAMED_REG(luau_x64_reg_esi, esi)
NAMED_REG(luau_x64_reg_edi, edi)
NAMED_REG(luau_x64_reg_r8d, r8d)
NAMED_REG(luau_x64_reg_r9d, r9d)
NAMED_REG(luau_x64_reg_r10d, r10d)
NAMED_REG(luau_x64_reg_r11d, r11d)
NAMED_REG(luau_x64_reg_r12d, r12d)
NAMED_REG(luau_x64_reg_r13d, r13d)
NAMED_REG(luau_x64_reg_r14d, r14d)
NAMED_REG(luau_x64_reg_r15d, r15d)

NAMED_REG(luau_x64_reg_rax, rax)
NAMED_REG(luau_x64_reg_rcx, rcx)
NAMED_REG(luau_x64_reg_rdx, rdx)
NAMED_REG(luau_x64_reg_rbx, rbx)
NAMED_REG(luau_x64_reg_rsp, rsp)
NAMED_REG(luau_x64_reg_rbp, rbp)
NAMED_REG(luau_x64_reg_rsi, rsi)
NAMED_REG(luau_x64_reg_rdi, rdi)
NAMED_REG(luau_x64_reg_r8, r8)
NAMED_REG(luau_x64_reg_r9, r9)
NAMED_REG(luau_x64_reg_r10, r10)
NAMED_REG(luau_x64_reg_r11, r11)
NAMED_REG(luau_x64_reg_r12, r12)
NAMED_REG(luau_x64_reg_r13, r13)
NAMED_REG(luau_x64_reg_r14, r14)
NAMED_REG(luau_x64_reg_r15, r15)

NAMED_REG(luau_x64_reg_xmm0, xmm0)
NAMED_REG(luau_x64_reg_xmm1, xmm1)
NAMED_REG(luau_x64_reg_xmm2, xmm2)
NAMED_REG(luau_x64_reg_xmm3, xmm3)
NAMED_REG(luau_x64_reg_xmm4, xmm4)
NAMED_REG(luau_x64_reg_xmm5, xmm5)
NAMED_REG(luau_x64_reg_xmm6, xmm6)
NAMED_REG(luau_x64_reg_xmm7, xmm7)
NAMED_REG(luau_x64_reg_xmm8, xmm8)
NAMED_REG(luau_x64_reg_xmm9, xmm9)
NAMED_REG(luau_x64_reg_xmm10, xmm10)
NAMED_REG(luau_x64_reg_xmm11, xmm11)
NAMED_REG(luau_x64_reg_xmm12, xmm12)
NAMED_REG(luau_x64_reg_xmm13, xmm13)
NAMED_REG(luau_x64_reg_xmm14, xmm14)
NAMED_REG(luau_x64_reg_xmm15, xmm15)

NAMED_REG(luau_x64_reg_ymm0, ymm0)
NAMED_REG(luau_x64_reg_ymm1, ymm1)
NAMED_REG(luau_x64_reg_ymm2, ymm2)
NAMED_REG(luau_x64_reg_ymm3, ymm3)
NAMED_REG(luau_x64_reg_ymm4, ymm4)
NAMED_REG(luau_x64_reg_ymm5, ymm5)
NAMED_REG(luau_x64_reg_ymm6, ymm6)
NAMED_REG(luau_x64_reg_ymm7, ymm7)
NAMED_REG(luau_x64_reg_ymm8, ymm8)
NAMED_REG(luau_x64_reg_ymm9, ymm9)
NAMED_REG(luau_x64_reg_ymm10, ymm10)
NAMED_REG(luau_x64_reg_ymm11, ymm11)
NAMED_REG(luau_x64_reg_ymm12, ymm12)
NAMED_REG(luau_x64_reg_ymm13, ymm13)
NAMED_REG(luau_x64_reg_ymm14, ymm14)
NAMED_REG(luau_x64_reg_ymm15, ymm15)

#undef NAMED_REG

// ---- operand handles -----------------------------------------------------

extern "C" LuauX64Operand* luau_x64_op_reg(const LuauX64Reg* r) {
    return newOp(OperandX64(regOf(r)));
}

extern "C" LuauX64Operand* luau_x64_imm(int32_t imm) {
    return newOp(OperandX64(imm));
}

extern "C" LuauX64Operand* luau_x64_op_mem(
    LuauX64Size size,
    const LuauX64Reg* index,
    uint8_t scale,
    const LuauX64Reg* base,
    int32_t disp) {
    return newOp(OperandX64(
        static_cast<SizeX64>(size),
        regOf(index),
        scale ? scale : 1,
        regOf(base),
        disp));
}

extern "C" LuauX64Operand* luau_x64_op_clone(const LuauX64Operand* o) {
    return o ? newOp(o->value) : nullptr;
}

extern "C" void luau_x64_op_free(LuauX64Operand* o) {
    delete o;
}

// ---- label handles -------------------------------------------------------

extern "C" LuauX64Label* luau_x64_label_new(void) {
    return new (std::nothrow) LuauX64Label{Label{}};
}

extern "C" void luau_x64_label_free(LuauX64Label* l) {
    delete l;
}

extern "C" uint32_t luau_x64_label_id(const LuauX64Label* l) {
    return l->value.id;
}

extern "C" uint32_t luau_x64_label_location(const LuauX64Label* l) {
    return l->value.location;
}

// ---- builder lifetime ----------------------------------------------------

extern "C" LuauX64Asm* luau_x64_asm_new_abi(int log_text, LuauX64ABI abi, unsigned int features) {
    try {
        return new LuauX64Asm(log_text != 0, static_cast<ABIX64>(abi), features);
    } catch (...) {
        return nullptr;
    }
}

extern "C" LuauX64Asm* luau_x64_asm_new(int log_text, unsigned int features) {
    try {
        return new LuauX64Asm(log_text != 0, features);
    } catch (...) {
        return nullptr;
    }
}

extern "C" void luau_x64_asm_free(LuauX64Asm* a) {
    delete a;
}

// ---- emit helper macros --------------------------------------------------

#define OP2(name, method)                                                                  \
    extern "C" void name(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs) { \
        try { a->builder.method(opOf(lhs), opOf(rhs)); } catch (...) {}                     \
    }

#define OP1(name, method)                                                                  \
    extern "C" void name(LuauX64Asm* a, const LuauX64Operand* op) {                         \
        try { a->builder.method(opOf(op)); } catch (...) {}                                 \
    }

#define AVX3(name, method)                                                                 \
    extern "C" void name(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2) { \
        try { a->builder.method(opOf(dst), opOf(src1), opOf(src2)); } catch (...) {}        \
    }

#define AVX2(name, method)                                                                 \
    extern "C" void name(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src) { \
        try { a->builder.method(opOf(dst), opOf(src)); } catch (...) {}                     \
    }

// ---- base two-operand ----------------------------------------------------

OP2(luau_x64_asm_add, add)
OP2(luau_x64_asm_sub, sub)
OP2(luau_x64_asm_cmp, cmp)
OP2(luau_x64_asm_and, and_)
OP2(luau_x64_asm_or, or_)
OP2(luau_x64_asm_xor, xor_)

// ---- shifts --------------------------------------------------------------

OP2(luau_x64_asm_sal, sal)
OP2(luau_x64_asm_sar, sar)
OP2(luau_x64_asm_shl, shl)
OP2(luau_x64_asm_shr, shr)
OP2(luau_x64_asm_rol, rol)
OP2(luau_x64_asm_ror, ror)

// ---- mov family ----------------------------------------------------------

OP2(luau_x64_asm_mov, mov)

extern "C" void luau_x64_asm_mov64(LuauX64Asm* a, const LuauX64Reg* lhs, int64_t imm) {
    try { a->builder.mov64(regOf(lhs), imm); } catch (...) {}
}
extern "C" void luau_x64_asm_movsx(LuauX64Asm* a, const LuauX64Reg* lhs, const LuauX64Operand* rhs) {
    try { a->builder.movsx(regOf(lhs), opOf(rhs)); } catch (...) {}
}
extern "C" void luau_x64_asm_movzx(LuauX64Asm* a, const LuauX64Reg* lhs, const LuauX64Operand* rhs) {
    try { a->builder.movzx(regOf(lhs), opOf(rhs)); } catch (...) {}
}

// ---- one-operand ---------------------------------------------------------

OP1(luau_x64_asm_div, div)
OP1(luau_x64_asm_idiv, idiv)
OP1(luau_x64_asm_mul, mul)
OP1(luau_x64_asm_imul1, imul)
OP1(luau_x64_asm_neg, neg)
OP1(luau_x64_asm_not, not_)
OP1(luau_x64_asm_dec, dec)
OP1(luau_x64_asm_inc, inc)
OP1(luau_x64_asm_push, push)
OP1(luau_x64_asm_pop, pop)

OP2(luau_x64_asm_imul2, imul)
OP2(luau_x64_asm_test, test)
OP2(luau_x64_asm_lea, lea)

extern "C" void luau_x64_asm_imul3(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* lhs, int32_t rhs) {
    try { a->builder.imul(opOf(dst), opOf(lhs), rhs); } catch (...) {}
}

extern "C" void luau_x64_asm_setcc(LuauX64Asm* a, LuauX64Cond cond, const LuauX64Operand* op) {
    try { a->builder.setcc(toCond(cond), opOf(op)); } catch (...) {}
}
extern "C" void luau_x64_asm_cmov(LuauX64Asm* a, LuauX64Cond cond, const LuauX64Reg* lhs, const LuauX64Operand* rhs) {
    try { a->builder.cmov(toCond(cond), regOf(lhs), opOf(rhs)); } catch (...) {}
}

extern "C" void luau_x64_asm_ret(LuauX64Asm* a) {
    try { a->builder.ret(); } catch (...) {}
}

// ---- control flow --------------------------------------------------------

extern "C" void luau_x64_asm_jcc(LuauX64Asm* a, LuauX64Cond cond, LuauX64Label* label) {
    try { a->builder.jcc(toCond(cond), label->value); } catch (...) {}
}
extern "C" void luau_x64_asm_jmp_label(LuauX64Asm* a, LuauX64Label* label) {
    try { a->builder.jmp(label->value); } catch (...) {}
}
extern "C" void luau_x64_asm_jmp_op(LuauX64Asm* a, const LuauX64Operand* op) {
    try { a->builder.jmp(opOf(op)); } catch (...) {}
}
extern "C" void luau_x64_asm_call_label(LuauX64Asm* a, LuauX64Label* label) {
    try { a->builder.call(label->value); } catch (...) {}
}
extern "C" void luau_x64_asm_call_op(LuauX64Asm* a, const LuauX64Operand* op) {
    try { a->builder.call(opOf(op)); } catch (...) {}
}
extern "C" void luau_x64_asm_lea_label(LuauX64Asm* a, const LuauX64Reg* lhs, LuauX64Label* label) {
    try { a->builder.lea(regOf(lhs), label->value); } catch (...) {}
}

extern "C" void luau_x64_asm_int3(LuauX64Asm* a) {
    try { a->builder.int3(); } catch (...) {}
}
extern "C" void luau_x64_asm_ud2(LuauX64Asm* a) {
    try { a->builder.ud2(); } catch (...) {}
}
extern "C" void luau_x64_asm_cqo(LuauX64Asm* a) {
    try { a->builder.cqo(); } catch (...) {}
}
extern "C" void luau_x64_asm_cdq(LuauX64Asm* a) {
    try { a->builder.cdq(); } catch (...) {}
}

extern "C" void luau_x64_asm_bsr(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Operand* src) {
    try { a->builder.bsr(regOf(dst), opOf(src)); } catch (...) {}
}
extern "C" void luau_x64_asm_bsf(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Operand* src) {
    try { a->builder.bsf(regOf(dst), opOf(src)); } catch (...) {}
}
extern "C" void luau_x64_asm_bswap(LuauX64Asm* a, const LuauX64Reg* dst) {
    try { a->builder.bswap(regOf(dst)); } catch (...) {}
}

// ---- alignment -----------------------------------------------------------

extern "C" void luau_x64_asm_nop(LuauX64Asm* a, uint32_t length) {
    try { a->builder.nop(length ? length : 1); } catch (...) {}
}
extern "C" void luau_x64_asm_align(LuauX64Asm* a, uint32_t alignment, LuauX64AlignmentData data) {
    try { a->builder.align(alignment, static_cast<AlignmentDataX64>(data)); } catch (...) {}
}

// ---- AVX -----------------------------------------------------------------

AVX3(luau_x64_asm_vaddpd, vaddpd)
AVX3(luau_x64_asm_vaddps, vaddps)
AVX3(luau_x64_asm_vaddsd, vaddsd)
AVX3(luau_x64_asm_vaddss, vaddss)

AVX3(luau_x64_asm_vsubsd, vsubsd)
AVX3(luau_x64_asm_vsubss, vsubss)
AVX3(luau_x64_asm_vsubps, vsubps)
AVX3(luau_x64_asm_vmulsd, vmulsd)
AVX3(luau_x64_asm_vmulss, vmulss)
AVX3(luau_x64_asm_vmulps, vmulps)
AVX3(luau_x64_asm_vdivsd, vdivsd)
AVX3(luau_x64_asm_vdivss, vdivss)
AVX3(luau_x64_asm_vdivps, vdivps)

AVX3(luau_x64_asm_vandps, vandps)
AVX3(luau_x64_asm_vandpd, vandpd)
AVX3(luau_x64_asm_vandnpd, vandnpd)

AVX3(luau_x64_asm_vxorps, vxorps)
AVX3(luau_x64_asm_vxorpd, vxorpd)
AVX3(luau_x64_asm_vorps, vorps)
AVX3(luau_x64_asm_vorpd, vorpd)

extern "C" void luau_x64_asm_vucomisd(LuauX64Asm* a, const LuauX64Operand* src1, const LuauX64Operand* src2) {
    try { a->builder.vucomisd(opOf(src1), opOf(src2)); } catch (...) {}
}
extern "C" void luau_x64_asm_vucomiss(LuauX64Asm* a, const LuauX64Operand* src1, const LuauX64Operand* src2) {
    try { a->builder.vucomiss(opOf(src1), opOf(src2)); } catch (...) {}
}

AVX2(luau_x64_asm_vcvttsd2si, vcvttsd2si)
AVX3(luau_x64_asm_vcvtsi2sd, vcvtsi2sd)
AVX3(luau_x64_asm_vcvtsi2ss, vcvtsi2ss)
AVX3(luau_x64_asm_vcvtsd2ss, vcvtsd2ss)
AVX3(luau_x64_asm_vcvtss2sd, vcvtss2sd)

extern "C" void luau_x64_asm_vroundsd(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, LuauX64RoundingMode mode) {
    try { a->builder.vroundsd(opOf(dst), opOf(src1), opOf(src2), toRounding(mode)); } catch (...) {}
}
extern "C" void luau_x64_asm_vroundss(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, LuauX64RoundingMode mode) {
    try { a->builder.vroundss(opOf(dst), opOf(src1), opOf(src2), toRounding(mode)); } catch (...) {}
}
extern "C" void luau_x64_asm_vroundps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src, LuauX64RoundingMode mode) {
    try { a->builder.vroundps(opOf(dst), opOf(src), toRounding(mode)); } catch (...) {}
}

AVX2(luau_x64_asm_vsqrtpd, vsqrtpd)
AVX2(luau_x64_asm_vsqrtps, vsqrtps)
AVX3(luau_x64_asm_vsqrtsd, vsqrtsd)
AVX3(luau_x64_asm_vsqrtss, vsqrtss)

AVX2(luau_x64_asm_vmovsd2, vmovsd)
AVX3(luau_x64_asm_vmovsd3, vmovsd)
AVX2(luau_x64_asm_vmovss2, vmovss)
AVX3(luau_x64_asm_vmovss3, vmovss)
AVX2(luau_x64_asm_vmovapd, vmovapd)
AVX2(luau_x64_asm_vmovaps, vmovaps)
AVX2(luau_x64_asm_vmovupd, vmovupd)
AVX2(luau_x64_asm_vmovups, vmovups)

extern "C" void luau_x64_asm_vmovq(LuauX64Asm* a, const LuauX64Operand* lhs, const LuauX64Operand* rhs) {
    try { a->builder.vmovq(opOf(lhs), opOf(rhs)); } catch (...) {}
}

AVX3(luau_x64_asm_vmaxps, vmaxps)
AVX3(luau_x64_asm_vmaxsd, vmaxsd)
AVX3(luau_x64_asm_vmaxss, vmaxss)
AVX3(luau_x64_asm_vminps, vminps)
AVX3(luau_x64_asm_vminsd, vminsd)
AVX3(luau_x64_asm_vminss, vminss)

AVX3(luau_x64_asm_vcmpeqsd, vcmpeqsd)
AVX3(luau_x64_asm_vcmpltsd, vcmpltsd)
AVX3(luau_x64_asm_vcmpltss, vcmpltss)
AVX3(luau_x64_asm_vcmpeqps, vcmpeqps)

extern "C" void luau_x64_asm_vblendvps(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, const LuauX64Reg* mask) {
    try { a->builder.vblendvps(regOf(dst), regOf(src1), opOf(src2), regOf(mask)); } catch (...) {}
}
extern "C" void luau_x64_asm_vblendvpd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, const LuauX64Reg* mask) {
    try { a->builder.vblendvpd(regOf(dst), regOf(src1), opOf(src2), regOf(mask)); } catch (...) {}
}

extern "C" void luau_x64_asm_vpshufps(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, uint8_t shuffle) {
    try { a->builder.vpshufps(regOf(dst), regOf(src1), opOf(src2), shuffle); } catch (...) {}
}
extern "C" void luau_x64_asm_vpinsrd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src1, const LuauX64Operand* src2, uint8_t offset) {
    try { a->builder.vpinsrd(regOf(dst), regOf(src1), opOf(src2), offset); } catch (...) {}
}
extern "C" void luau_x64_asm_vpextrd(LuauX64Asm* a, const LuauX64Reg* dst, const LuauX64Reg* src, uint8_t offset) {
    try { a->builder.vpextrd(regOf(dst), regOf(src), offset); } catch (...) {}
}

extern "C" void luau_x64_asm_vdpps(LuauX64Asm* a, const LuauX64Operand* dst, const LuauX64Operand* src1, const LuauX64Operand* src2, uint8_t mask) {
    try { a->builder.vdpps(opOf(dst), opOf(src1), opOf(src2), mask); } catch (...) {}
}
AVX3(luau_x64_asm_vfmadd213ps, vfmadd213ps)
AVX3(luau_x64_asm_vfmadd213pd, vfmadd213pd)

#undef OP2
#undef OP1
#undef AVX3
#undef AVX2

// ---- labels (builder side) ----------------------------------------------

extern "C" void luau_x64_asm_set_label_here(LuauX64Asm* a, LuauX64Label* out) {
    try { out->value = a->builder.setLabel(); } catch (...) {}
}
extern "C" void luau_x64_asm_place_label(LuauX64Asm* a, LuauX64Label* label) {
    try { a->builder.setLabel(label->value); } catch (...) {}
}
extern "C" uint32_t luau_x64_asm_label_offset(LuauX64Asm* a, const LuauX64Label* label) {
    try { return a->builder.getLabelOffset(label->value); } catch (...) { return 0; }
}

// ---- constant allocation -------------------------------------------------

extern "C" LuauX64Operand* luau_x64_asm_i32(LuauX64Asm* a, int32_t value) {
    try { return newOp(a->builder.i32(value)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_i64(LuauX64Asm* a, int64_t value) {
    try { return newOp(a->builder.i64(value)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_f32(LuauX64Asm* a, float value) {
    try { return newOp(a->builder.f32(value)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_f64(LuauX64Asm* a, double value) {
    try { return newOp(a->builder.f64(value)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_u32x4(LuauX64Asm* a, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    try { return newOp(a->builder.u32x4(x, y, z, w)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_f32x4(LuauX64Asm* a, float x, float y, float z, float w) {
    try { return newOp(a->builder.f32x4(x, y, z, w)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_f64x2(LuauX64Asm* a, double x, double y) {
    try { return newOp(a->builder.f64x2(x, y)); } catch (...) { return nullptr; }
}
extern "C" LuauX64Operand* luau_x64_asm_bytes(LuauX64Asm* a, const void* ptr, size_t size, size_t align) {
    try { return newOp(a->builder.bytes(ptr, size, align ? align : 8)); } catch (...) { return nullptr; }
}

// ---- finalize / output ---------------------------------------------------

extern "C" int luau_x64_asm_finalize(LuauX64Asm* a) {
    try { return a->builder.finalize() ? 1 : 0; } catch (...) { return 0; }
}
extern "C" uint32_t luau_x64_asm_code_size(LuauX64Asm* a) {
    return a->builder.getCodeSize();
}
extern "C" unsigned int luau_x64_asm_instruction_count(LuauX64Asm* a) {
    return a->builder.getInstructionCount();
}
extern "C" const uint8_t* luau_x64_asm_code_ptr(LuauX64Asm* a, size_t* out_len) {
    if (out_len)
        *out_len = a->builder.code.size();
    return a->builder.code.data();
}
extern "C" const uint8_t* luau_x64_asm_data_ptr(LuauX64Asm* a, size_t* out_len) {
    if (out_len)
        *out_len = a->builder.data.size();
    return a->builder.data.data();
}
extern "C" char* luau_x64_asm_text(LuauX64Asm* a) {
    const std::string& t = a->builder.text;
    char* out = static_cast<char*>(std::malloc(t.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, t.data(), t.size());
    out[t.size()] = '\0';
    return out;
}
extern "C" void luau_x64_asm_log_append(LuauX64Asm* a, const char* text) {
    try { a->builder.logAppend("%s", text ? text : ""); } catch (...) {}
}
