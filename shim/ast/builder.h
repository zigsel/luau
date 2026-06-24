// Shim: Luau AST construction + compilation (the transpiler path).
//
// Build an AST out of opaque nodes via an arena-owning builder, then compile
// it straight to Luau bytecode. All node pointers are LuauAstNode*; they live
// in the builder's arena and are freed when the builder is freed.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Binary operators (mirror Luau::AstExprBinary::Op).
typedef enum LuauAstBinaryOp {
    LUAU_AST_BINOP_ADD = 0,
    LUAU_AST_BINOP_SUB = 1,
    LUAU_AST_BINOP_MUL = 2,
    LUAU_AST_BINOP_DIV = 3,
    LUAU_AST_BINOP_FLOORDIV = 4,
    LUAU_AST_BINOP_MOD = 5,
    LUAU_AST_BINOP_POW = 6,
    LUAU_AST_BINOP_CONCAT = 7,
    LUAU_AST_BINOP_COMPARE_NE = 8,
    LUAU_AST_BINOP_COMPARE_EQ = 9,
    LUAU_AST_BINOP_COMPARE_LT = 10,
    LUAU_AST_BINOP_COMPARE_LE = 11,
    LUAU_AST_BINOP_COMPARE_GT = 12,
    LUAU_AST_BINOP_COMPARE_GE = 13,
    LUAU_AST_BINOP_AND = 14,
    LUAU_AST_BINOP_OR = 15,
} LuauAstBinaryOp;

// Unary operators (mirror Luau::AstExprUnary::Op).
typedef enum LuauAstUnaryOp {
    LUAU_AST_UNOP_NOT = 0,
    LUAU_AST_UNOP_MINUS = 1,
    LUAU_AST_UNOP_LEN = 2,
} LuauAstUnaryOp;

// Table item kinds (mirror Luau::AstExprTable::Item::Kind).
typedef enum LuauAstTableItemKind {
    LUAU_AST_TABLEITEM_LIST = 0,    // value only (key is null)
    LUAU_AST_TABLEITEM_RECORD = 1,  // key=value, key is a constant string name
    LUAU_AST_TABLEITEM_GENERAL = 2, // [key]=value
} LuauAstTableItemKind;

// Create / destroy a builder (owns an arena allocator + name table).
LuauAstBuilder* luau_astbuild_new(void);
void luau_astbuild_free(LuauAstBuilder* b);

// --- locals & types --------------------------------------------------------

// Declare an AstLocal by name (with an optional type annotation, may be NULL).
// Returns an opaque local handle usable by local/function/for nodes.
LuauAstNode* luau_astbuild_local(LuauAstBuilder* b, const char* name, LuauAstNode* annotation);

// A basic type reference by name (e.g. `number`, `string`, `MyType`).
LuauAstNode* luau_astbuild_type_reference(LuauAstBuilder* b, const char* name);

// Expression node factories.
LuauAstNode* luau_astbuild_constant_nil(LuauAstBuilder* b);
LuauAstNode* luau_astbuild_constant_bool(LuauAstBuilder* b, int value);
LuauAstNode* luau_astbuild_constant_number(LuauAstBuilder* b, double value);
LuauAstNode* luau_astbuild_constant_string(LuauAstBuilder* b, const char* s, size_t len);
LuauAstNode* luau_astbuild_global(LuauAstBuilder* b, const char* name);
LuauAstNode* luau_astbuild_binary(LuauAstBuilder* b, int op, LuauAstNode* lhs, LuauAstNode* rhs);
LuauAstNode* luau_astbuild_unary(LuauAstBuilder* b, int op, LuauAstNode* e);
LuauAstNode* luau_astbuild_call(LuauAstBuilder* b, LuauAstNode* func, LuauAstNode** args, int nargs);
LuauAstNode* luau_astbuild_index_name(LuauAstBuilder* b, LuauAstNode* expr, const char* name);
LuauAstNode* luau_astbuild_group(LuauAstBuilder* b, LuauAstNode* e);
LuauAstNode* luau_astbuild_constant_integer(LuauAstBuilder* b, long long value);
LuauAstNode* luau_astbuild_expr_local(LuauAstBuilder* b, LuauAstNode* local);
LuauAstNode* luau_astbuild_varargs(LuauAstBuilder* b);
LuauAstNode* luau_astbuild_index_expr(LuauAstBuilder* b, LuauAstNode* expr, LuauAstNode* index);
// `args`/`body` are arrays of locals/stats; body is built into an AstStatBlock.
LuauAstNode* luau_astbuild_function(
    LuauAstBuilder* b, LuauAstNode** args, int nargs, int vararg, LuauAstNode** body, int nbody);
// Build a table. Each item i is (kind[i], keys[i] may be NULL, values[i]).
LuauAstNode* luau_astbuild_table(
    LuauAstBuilder* b, int* kinds, LuauAstNode** keys, LuauAstNode** values, int nitems);
LuauAstNode* luau_astbuild_type_assertion(LuauAstBuilder* b, LuauAstNode* expr, LuauAstNode* annotation);
LuauAstNode* luau_astbuild_if_else(
    LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* trueExpr, LuauAstNode* falseExpr);
// Interpolated string: `nstrings` raw chunks and `nexprs` (== nstrings-1) embedded exprs.
LuauAstNode* luau_astbuild_interp_string(
    LuauAstBuilder* b, const char** strings, size_t* lens, int nstrings, LuauAstNode** exprs, int nexprs);

// Statement node factories.
LuauAstNode* luau_astbuild_return(LuauAstBuilder* b, LuauAstNode** exprs, int nexprs);
LuauAstNode* luau_astbuild_expr_stat(LuauAstBuilder* b, LuauAstNode* e);
LuauAstNode* luau_astbuild_block(LuauAstBuilder* b, LuauAstNode** stats, int nstats);
// `thenBlock` must be a block node; `elseStat` may be NULL or any stat (block / nested if).
LuauAstNode* luau_astbuild_if(LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* thenBlock, LuauAstNode* elseStat);
LuauAstNode* luau_astbuild_while(LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* body);
LuauAstNode* luau_astbuild_repeat(LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* body);
LuauAstNode* luau_astbuild_break(LuauAstBuilder* b);
LuauAstNode* luau_astbuild_continue(LuauAstBuilder* b);
// Numeric for; `step` may be NULL.
LuauAstNode* luau_astbuild_for(
    LuauAstBuilder* b, LuauAstNode* var, LuauAstNode* from, LuauAstNode* to, LuauAstNode* step, LuauAstNode* body);
LuauAstNode* luau_astbuild_for_in(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues, LuauAstNode* body);
LuauAstNode* luau_astbuild_assign(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues);
LuauAstNode* luau_astbuild_compound_assign(LuauAstBuilder* b, int op, LuauAstNode* var, LuauAstNode* value);
LuauAstNode* luau_astbuild_local_stat(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues);
LuauAstNode* luau_astbuild_function_stat(LuauAstBuilder* b, LuauAstNode* name, LuauAstNode* func);
LuauAstNode* luau_astbuild_local_function(LuauAstBuilder* b, LuauAstNode* name, LuauAstNode* func);

// Compile `rootBlock` (an AstStatBlock node) to bytecode.
// On success returns a malloc'd byte buffer (caller frees) and sets *out_len.
// On failure returns NULL and sets *out_err to a malloc'd message (caller frees).
char* luau_astbuild_compile(LuauAstBuilder* b, LuauAstNode* rootBlock, int* out_len, char** out_err);

LUAU_END_DECLS
