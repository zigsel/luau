// extern "C" shim over Luau::BytecodeBuilder (Bytecode module).

#include "bytecode.h"

#include "Luau/BytecodeBuilder.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace Luau;

struct LuauBytecodeBuilder {
    BytecodeBuilder builder;
    // BytecodeBuilder keeps StringRefs by pointer until finalize(); keep the
    // backing strings alive here.
    std::vector<std::string> strings;
    // Scratch buffer that backs the const char* returned by dump/annotate/etc.
    // Valid until the next such call on this handle or until free.
    std::string scratch;
    // Caches that back the count/at accessors for vector-returning methods.
    std::vector<uint32_t> expandedJumps;
    std::vector<std::string_view> stringTableCache;

    BytecodeBuilder::StringRef keep(const char* s, size_t len) {
        strings.emplace_back(s, len);
        const std::string& kept = strings.back();
        return BytecodeBuilder::StringRef{kept.data(), kept.size()};
    }
    const char* hold(std::string&& v, size_t* out_len) {
        scratch = std::move(v);
        if (out_len) *out_len = scratch.size();
        return scratch.data();
    }
};

extern "C" LuauBytecodeBuilder* luau_bcb_new(void) {
    return new LuauBytecodeBuilder();
}
extern "C" void luau_bcb_free(LuauBytecodeBuilder* b) {
    delete b;
}

extern "C" unsigned int luau_bcb_begin_function(LuauBytecodeBuilder* b, unsigned char numparams, int isvararg) {
    return b->builder.beginFunction(numparams, isvararg != 0);
}
extern "C" void luau_bcb_end_function(LuauBytecodeBuilder* b, unsigned char maxstacksize, unsigned char numupvalues) {
    b->builder.endFunction(maxstacksize, numupvalues);
}
extern "C" void luau_bcb_set_main_function(LuauBytecodeBuilder* b, unsigned int fid) {
    b->builder.setMainFunction(fid);
}

extern "C" int luau_bcb_add_constant_nil(LuauBytecodeBuilder* b) {
    return b->builder.addConstantNil();
}
extern "C" int luau_bcb_add_constant_boolean(LuauBytecodeBuilder* b, int value) {
    return b->builder.addConstantBoolean(value != 0);
}
extern "C" int luau_bcb_add_constant_number(LuauBytecodeBuilder* b, double value) {
    return b->builder.addConstantNumber(value);
}
extern "C" int luau_bcb_add_constant_string(LuauBytecodeBuilder* b, const char* s, size_t len) {
    b->strings.emplace_back(s, len);
    const std::string& kept = b->strings.back();
    BytecodeBuilder::StringRef ref{kept.data(), kept.size()};
    return b->builder.addConstantString(ref);
}

extern "C" void luau_bcb_emit_abc(LuauBytecodeBuilder* b, unsigned char op, unsigned char a, unsigned char bb, unsigned char c) {
    b->builder.emitABC(static_cast<LuauOpcode>(op), a, bb, c);
}
extern "C" void luau_bcb_emit_ad(LuauBytecodeBuilder* b, unsigned char op, unsigned char a, short d) {
    b->builder.emitAD(static_cast<LuauOpcode>(op), a, d);
}
extern "C" void luau_bcb_emit_e(LuauBytecodeBuilder* b, unsigned char op, int e) {
    b->builder.emitE(static_cast<LuauOpcode>(op), e);
}
extern "C" void luau_bcb_emit_aux(LuauBytecodeBuilder* b, unsigned int aux) {
    b->builder.emitAux(aux);
}

extern "C" void luau_bcb_set_debug_function_name(LuauBytecodeBuilder* b, const char* s, size_t len) {
    b->strings.emplace_back(s, len);
    const std::string& kept = b->strings.back();
    BytecodeBuilder::StringRef ref{kept.data(), kept.size()};
    b->builder.setDebugFunctionName(ref);
}
extern "C" void luau_bcb_set_debug_line(LuauBytecodeBuilder* b, int line) {
    b->builder.setDebugLine(line);
}

extern "C" void luau_bcb_finalize(LuauBytecodeBuilder* b) {
    b->builder.finalize();
}
extern "C" const char* luau_bcb_get_bytecode(LuauBytecodeBuilder* b, size_t* out_len) {
    const std::string& bc = b->builder.getBytecode();
    if (out_len) *out_len = bc.size();
    return bc.data();
}

// --- appended: full BytecodeBuilder surface ---

extern "C" void luau_bcb_end_function_flags(LuauBytecodeBuilder* b, unsigned char maxstacksize, unsigned char numupvalues, unsigned char flags) {
    b->builder.endFunction(maxstacksize, numupvalues, flags);
}

extern "C" int luau_bcb_add_constant_integer(LuauBytecodeBuilder* b, long long value) {
    return b->builder.addConstantInteger(static_cast<int64_t>(value));
}
extern "C" int luau_bcb_add_constant_vector(LuauBytecodeBuilder* b, float x, float y, float z, float w) {
    return b->builder.addConstantVector(x, y, z, w);
}
extern "C" int luau_bcb_add_import(LuauBytecodeBuilder* b, unsigned int iid) {
    return b->builder.addImport(iid);
}
extern "C" int luau_bcb_add_constant_closure(LuauBytecodeBuilder* b, unsigned int fid) {
    return b->builder.addConstantClosure(fid);
}
extern "C" short luau_bcb_add_child_function(LuauBytecodeBuilder* b, unsigned int fid) {
    return b->builder.addChildFunction(fid);
}

extern "C" unsigned int luau_bcb_add_fb_slot(LuauBytecodeBuilder* b, int t) {
    return b->builder.addFbSlot(static_cast<LuauFeedbackType>(t));
}

extern "C" void luau_bcb_undo_emit(LuauBytecodeBuilder* b, unsigned char op) {
    b->builder.undoEmit(static_cast<LuauOpcode>(op));
}
extern "C" size_t luau_bcb_emit_label(LuauBytecodeBuilder* b) {
    return b->builder.emitLabel();
}
extern "C" int luau_bcb_patch_jump_d(LuauBytecodeBuilder* b, size_t jump_label, size_t target_label) {
    return b->builder.patchJumpD(jump_label, target_label) ? 1 : 0;
}
extern "C" int luau_bcb_patch_skip_c(LuauBytecodeBuilder* b, size_t jump_label, size_t target_label) {
    return b->builder.patchSkipC(jump_label, target_label) ? 1 : 0;
}
extern "C" void luau_bcb_patch_aux(LuauBytecodeBuilder* b, size_t target_aux, int new_value) {
    b->builder.patchAux(target_aux, new_value);
}
extern "C" void luau_bcb_fold_jumps(LuauBytecodeBuilder* b) {
    b->builder.foldJumps();
}
extern "C" size_t luau_bcb_expand_jumps_count(LuauBytecodeBuilder* b) {
    // expandJumps() mutates the builder and returns the new instruction stream;
    // call once and cache so the _at accessor reads consistent data.
    b->expandedJumps = b->builder.expandJumps();
    return b->expandedJumps.size();
}
extern "C" unsigned int luau_bcb_expand_jumps_at(LuauBytecodeBuilder* b, size_t i) {
    return b->expandedJumps[i];
}

extern "C" void luau_bcb_set_function_type_info(LuauBytecodeBuilder* b, const char* s, size_t len) {
    b->builder.setFunctionTypeInfo(std::string(s, len));
}
extern "C" void luau_bcb_push_local_type_info(LuauBytecodeBuilder* b, int type, unsigned char reg, unsigned int startpc, unsigned int endpc) {
    b->builder.pushLocalTypeInfo(static_cast<LuauBytecodeType>(type), reg, startpc, endpc);
}
extern "C" void luau_bcb_push_upval_type_info(LuauBytecodeBuilder* b, int type) {
    b->builder.pushUpvalTypeInfo(static_cast<LuauBytecodeType>(type));
}

extern "C" unsigned int luau_bcb_add_userdata_type(LuauBytecodeBuilder* b, const char* name) {
    return b->builder.addUserdataType(name);
}
extern "C" void luau_bcb_use_userdata_type(LuauBytecodeBuilder* b, unsigned int index) {
    b->builder.useUserdataType(index);
}

extern "C" void luau_bcb_set_debug_function_line_defined(LuauBytecodeBuilder* b, int line) {
    b->builder.setDebugFunctionLineDefined(line);
}
extern "C" void luau_bcb_push_debug_local(LuauBytecodeBuilder* b, const char* s, size_t len, unsigned char reg, unsigned int startpc, unsigned int endpc) {
    b->builder.pushDebugLocal(b->keep(s, len), reg, startpc, endpc);
}
extern "C" void luau_bcb_push_debug_upval(LuauBytecodeBuilder* b, const char* s, size_t len) {
    b->builder.pushDebugUpval(b->keep(s, len));
}
extern "C" void luau_bcb_add_debug_remark(LuauBytecodeBuilder* b, const char* text) {
    // addDebugRemark is printf-style; pass text as a literal-free format.
    b->builder.addDebugRemark("%s", text);
}

extern "C" size_t luau_bcb_get_instruction_count(LuauBytecodeBuilder* b) {
    return b->builder.getInstructionCount();
}
extern "C" size_t luau_bcb_get_total_instruction_count(LuauBytecodeBuilder* b) {
    return b->builder.getTotalInstructionCount();
}
extern "C" unsigned int luau_bcb_get_debug_pc(LuauBytecodeBuilder* b) {
    return b->builder.getDebugPC();
}
extern "C" int luau_bcb_needs_debug_remarks(LuauBytecodeBuilder* b) {
    return b->builder.needsDebugRemarks() ? 1 : 0;
}

extern "C" void luau_bcb_set_dump_flags(LuauBytecodeBuilder* b, unsigned int flags) {
    b->builder.setDumpFlags(flags);
}
extern "C" void luau_bcb_set_dump_source(LuauBytecodeBuilder* b, const char* s, size_t len) {
    b->builder.setDumpSource(std::string(s, len));
}
extern "C" const char* luau_bcb_dump_function(LuauBytecodeBuilder* b, unsigned int id, size_t* out_len) {
    return b->hold(b->builder.dumpFunction(id), out_len);
}
extern "C" const char* luau_bcb_dump_everything(LuauBytecodeBuilder* b, size_t* out_len) {
    return b->hold(b->builder.dumpEverything(), out_len);
}
extern "C" const char* luau_bcb_dump_source_remarks(LuauBytecodeBuilder* b, size_t* out_len) {
    return b->hold(b->builder.dumpSourceRemarks(), out_len);
}
extern "C" const char* luau_bcb_dump_type_info(LuauBytecodeBuilder* b, size_t* out_len) {
    return b->hold(b->builder.dumpTypeInfo(), out_len);
}
extern "C" const char* luau_bcb_get_function_data(LuauBytecodeBuilder* b, unsigned int id, size_t* out_len) {
    return b->hold(b->builder.getFunctionData(id), out_len);
}

extern "C" size_t luau_bcb_string_table_count(LuauBytecodeBuilder* b) {
    // getStringTable() rebuilds the table each call; cache once so the _at
    // accessor reads consistent data.
    b->stringTableCache = b->builder.getStringTable();
    return b->stringTableCache.size();
}
extern "C" const char* luau_bcb_string_table_at(LuauBytecodeBuilder* b, size_t i, size_t* out_len) {
    if (i >= b->stringTableCache.size())
        b->stringTableCache = b->builder.getStringTable();
    if (out_len) *out_len = b->stringTableCache[i].size();
    return b->stringTableCache[i].data();
}

extern "C" const char* luau_bcb_annotate_instruction(LuauBytecodeBuilder* b, unsigned int fid, unsigned int instpos, size_t* out_len) {
    std::string result;
    b->builder.annotateInstruction(result, fid, instpos);
    return b->hold(std::move(result), out_len);
}

extern "C" unsigned int luau_bcb_get_import_id_1(int id0) {
    return BytecodeBuilder::getImportId(id0);
}
extern "C" unsigned int luau_bcb_get_import_id_2(int id0, int id1) {
    return BytecodeBuilder::getImportId(id0, id1);
}
extern "C" unsigned int luau_bcb_get_import_id_3(int id0, int id1, int id2) {
    return BytecodeBuilder::getImportId(id0, id1, id2);
}
extern "C" int luau_bcb_decompose_import_id(unsigned int ids, int* id0, int* id1, int* id2) {
    int32_t a = 0, c = 0, d = 0;
    int count = BytecodeBuilder::decomposeImportId(ids, a, c, d);
    if (id0) *id0 = a;
    if (id1) *id1 = c;
    if (id2) *id2 = d;
    return count;
}
extern "C" unsigned int luau_bcb_get_string_hash(const char* s, size_t len) {
    BytecodeBuilder::StringRef ref{s, len};
    return BytecodeBuilder::getStringHash(ref);
}
extern "C" char* luau_bcb_get_error(const char* message) {
    std::string e = BytecodeBuilder::getError(message ? message : "");
    char* out = static_cast<char*>(std::malloc(e.size() + 1));
    if (out) {
        std::memcpy(out, e.data(), e.size());
        out[e.size()] = '\0';
    }
    return out;
}
extern "C" void luau_bcb_free_string(char* s) {
    std::free(s);
}
extern "C" unsigned char luau_bcb_get_version(void) {
    return BytecodeBuilder::getVersion();
}
extern "C" unsigned char luau_bcb_get_type_encoding_version(void) {
    return BytecodeBuilder::getTypeEncodingVersion();
}

extern "C" int luau_bcb_add_constant_table(LuauBytecodeBuilder* b, const int* keys, const int* constants, int n) {
    try {
        BytecodeBuilder::TableShape shape;
        if (n < 0) n = 0;
        unsigned int max = BytecodeBuilder::TableShape::kMaxLength;
        if (static_cast<unsigned int>(n) > max) n = static_cast<int>(max);
        shape.length = static_cast<unsigned int>(n);
        shape.hasConstants = constants != nullptr;
        for (int i = 0; i < n; ++i) {
            shape.keys[i] = keys ? static_cast<int32_t>(keys[i]) : -1;
            shape.constants[i] = constants ? static_cast<int32_t>(constants[i]) : -1;
        }
        return b->builder.addConstantTable(shape);
    } catch (...) {
        return -1;
    }
}

extern "C" int luau_bcb_add_class_shape(LuauBytecodeBuilder* b, int class_name,
                                        const int* property_names, int property_count,
                                        const int* method_names, int method_count) {
    try {
        BytecodeBuilder::ClassShape shape;
        shape.className = static_cast<int32_t>(class_name);
        for (int i = 0; i < property_count; ++i)
            shape.propertyNames.push_back(static_cast<int32_t>(property_names[i]));
        for (int i = 0; i < method_count; ++i)
            shape.methodNames.push_back(static_cast<int32_t>(method_names[i]));
        return b->builder.addClassShape(std::move(shape));
    } catch (...) {
        return -1;
    }
}
