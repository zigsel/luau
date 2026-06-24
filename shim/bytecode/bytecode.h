// Shim: Luau Bytecode — a thin wrapper over Luau::BytecodeBuilder for emitting
// bytecode by hand.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

LuauBytecodeBuilder* luau_bcb_new(void);
void luau_bcb_free(LuauBytecodeBuilder* b);

unsigned int luau_bcb_begin_function(LuauBytecodeBuilder* b, unsigned char numparams, int isvararg);
void luau_bcb_end_function(LuauBytecodeBuilder* b, unsigned char maxstacksize, unsigned char numupvalues);
void luau_bcb_set_main_function(LuauBytecodeBuilder* b, unsigned int fid);

int luau_bcb_add_constant_nil(LuauBytecodeBuilder* b);
int luau_bcb_add_constant_boolean(LuauBytecodeBuilder* b, int value);
int luau_bcb_add_constant_number(LuauBytecodeBuilder* b, double value);
int luau_bcb_add_constant_string(LuauBytecodeBuilder* b, const char* s, size_t len);

void luau_bcb_emit_abc(LuauBytecodeBuilder* b, unsigned char op, unsigned char a, unsigned char bb, unsigned char c);
void luau_bcb_emit_ad(LuauBytecodeBuilder* b, unsigned char op, unsigned char a, short d);
void luau_bcb_emit_e(LuauBytecodeBuilder* b, unsigned char op, int e);
void luau_bcb_emit_aux(LuauBytecodeBuilder* b, unsigned int aux);

void luau_bcb_set_debug_function_name(LuauBytecodeBuilder* b, const char* s, size_t len);
void luau_bcb_set_debug_line(LuauBytecodeBuilder* b, int line);

void luau_bcb_finalize(LuauBytecodeBuilder* b);
const char* luau_bcb_get_bytecode(LuauBytecodeBuilder* b, size_t* out_len);

// --- end of original partial shim; appended full surface below ---

// endFunction has an optional `flags` argument (defaults to 0 in the original).
void luau_bcb_end_function_flags(LuauBytecodeBuilder* b, unsigned char maxstacksize, unsigned char numupvalues, unsigned char flags);

// Additional constant/import/closure/child entries.
int luau_bcb_add_constant_integer(LuauBytecodeBuilder* b, long long value);
int luau_bcb_add_constant_vector(LuauBytecodeBuilder* b, float x, float y, float z, float w);
int luau_bcb_add_import(LuauBytecodeBuilder* b, unsigned int iid);
int luau_bcb_add_constant_closure(LuauBytecodeBuilder* b, unsigned int fid);
short luau_bcb_add_child_function(LuauBytecodeBuilder* b, unsigned int fid);

// Feedback slot (LuauFeedbackType passed as int).
unsigned int luau_bcb_add_fb_slot(LuauBytecodeBuilder* b, int t);

// Instruction stream manipulation.
void luau_bcb_undo_emit(LuauBytecodeBuilder* b, unsigned char op);
size_t luau_bcb_emit_label(LuauBytecodeBuilder* b);
// patch* return [[nodiscard]] bool success.
int luau_bcb_patch_jump_d(LuauBytecodeBuilder* b, size_t jump_label, size_t target_label);
int luau_bcb_patch_skip_c(LuauBytecodeBuilder* b, size_t jump_label, size_t target_label);
void luau_bcb_patch_aux(LuauBytecodeBuilder* b, size_t target_aux, int new_value);
void luau_bcb_fold_jumps(LuauBytecodeBuilder* b);
// expandJumps() returns std::vector<uint32_t>: vector of instructions.
size_t luau_bcb_expand_jumps_count(LuauBytecodeBuilder* b);
unsigned int luau_bcb_expand_jumps_at(LuauBytecodeBuilder* b, size_t i);

// Type info.
void luau_bcb_set_function_type_info(LuauBytecodeBuilder* b, const char* s, size_t len);
// LuauBytecodeType passed as int (one byte enum).
void luau_bcb_push_local_type_info(LuauBytecodeBuilder* b, int type, unsigned char reg, unsigned int startpc, unsigned int endpc);
void luau_bcb_push_upval_type_info(LuauBytecodeBuilder* b, int type);

// Userdata types.
unsigned int luau_bcb_add_userdata_type(LuauBytecodeBuilder* b, const char* name);
void luau_bcb_use_userdata_type(LuauBytecodeBuilder* b, unsigned int index);

// Debug info.
void luau_bcb_set_debug_function_line_defined(LuauBytecodeBuilder* b, int line);
void luau_bcb_push_debug_local(LuauBytecodeBuilder* b, const char* s, size_t len, unsigned char reg, unsigned int startpc, unsigned int endpc);
void luau_bcb_push_debug_upval(LuauBytecodeBuilder* b, const char* s, size_t len);
void luau_bcb_add_debug_remark(LuauBytecodeBuilder* b, const char* text);

// Counters.
size_t luau_bcb_get_instruction_count(LuauBytecodeBuilder* b);
size_t luau_bcb_get_total_instruction_count(LuauBytecodeBuilder* b);
unsigned int luau_bcb_get_debug_pc(LuauBytecodeBuilder* b);
int luau_bcb_needs_debug_remarks(LuauBytecodeBuilder* b);

// Dump configuration / output. The returned const char* is owned by the handle
// and stays valid until the next dump call on the same handle or until free.
void luau_bcb_set_dump_flags(LuauBytecodeBuilder* b, unsigned int flags);
void luau_bcb_set_dump_source(LuauBytecodeBuilder* b, const char* s, size_t len);
const char* luau_bcb_dump_function(LuauBytecodeBuilder* b, unsigned int id, size_t* out_len);
const char* luau_bcb_dump_everything(LuauBytecodeBuilder* b, size_t* out_len);
const char* luau_bcb_dump_source_remarks(LuauBytecodeBuilder* b, size_t* out_len);
const char* luau_bcb_dump_type_info(LuauBytecodeBuilder* b, size_t* out_len);
const char* luau_bcb_get_function_data(LuauBytecodeBuilder* b, unsigned int id, size_t* out_len);

// String table: vector of string_view.
size_t luau_bcb_string_table_count(LuauBytecodeBuilder* b);
const char* luau_bcb_string_table_at(LuauBytecodeBuilder* b, size_t i, size_t* out_len);

// annotateInstruction appends to a string; we return a fresh owned string.
const char* luau_bcb_annotate_instruction(LuauBytecodeBuilder* b, unsigned int fid, unsigned int instpos, size_t* out_len);

// Static helpers (no builder instance required).
unsigned int luau_bcb_get_import_id_1(int id0);
unsigned int luau_bcb_get_import_id_2(int id0, int id1);
unsigned int luau_bcb_get_import_id_3(int id0, int id1, int id2);
// decomposeImportId returns the count and fills id0/id1/id2 out params.
int luau_bcb_decompose_import_id(unsigned int ids, int* id0, int* id1, int* id2);
unsigned int luau_bcb_get_string_hash(const char* s, size_t len);
// getError wraps a message; returns a malloc'd string the caller frees.
char* luau_bcb_get_error(const char* message);
void luau_bcb_free_string(char* s);
unsigned char luau_bcb_get_version(void);
unsigned char luau_bcb_get_type_encoding_version(void);

// --- TableShape / ClassShape constant entries ---
//
// TableShape holds up to BytecodeBuilder::TableShape::kMaxLength (32) keys, each
// being a proto-constant index, plus an optional parallel array of constant
// indices (with -1 sentinel meaning "no constant"). addConstantTable interns the
// shape and returns the constant id.
//
// Simple form: pass an array of key constant-indices. `constants` may be NULL
// (then hasConstants is false); if non-NULL it must have `n` entries (use -1 for
// "no constant" on a given key). `n` must be <= 32.
int luau_bcb_add_constant_table(LuauBytecodeBuilder* b, const int* keys, const int* constants, int n);

// ClassShape holds a className constant index plus vectors of property- and
// method-name constant indices. Either array may be NULL with its count 0.
int luau_bcb_add_class_shape(LuauBytecodeBuilder* b, int class_name,
                             const int* property_names, int property_count,
                             const int* method_names, int method_count);

// Note: setDumpFlags' member-pointer machinery and the protected
// validate*/write*/dump* internals are intentionally NOT shimmed.

LUAU_END_DECLS
