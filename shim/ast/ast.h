// Shim: Luau Ast — parsing, diagnostics, and flattened node walking.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- parsing & diagnostics ---------------------------------------------------

// Parse Luau `src`. Always returns a non-null handle; inspect errors with the
// accessors below. Free with `luau_ast_parse_free`.
LuauParseResult* luau_ast_parse(const char* src, size_t len);

int luau_ast_error_count(const LuauParseResult* r);
const char* luau_ast_error_message(const LuauParseResult* r, int i);
LuauPosition luau_ast_error_position(const LuauParseResult* r, int i);

size_t luau_ast_line_count(const LuauParseResult* r);
int luau_ast_has_root(const LuauParseResult* r);
int luau_ast_hotcomment_count(const LuauParseResult* r);
const char* luau_ast_hotcomment_content(const LuauParseResult* r, int i);

void luau_ast_parse_free(LuauParseResult* r);

// ---- node walking ------------------------------------------------------------
//
// After a parse the AST is flattened (depth-first) into an indexable node list.
// Each node has a stable kind, a parent index (-1 for the root), a source span,
// and optional string / number / integer / boolean payloads for leaves.

typedef enum LuauAstKind {
    LUAU_AST_UNKNOWN = 0,
    // statements
    LUAU_AST_STAT_BLOCK, LUAU_AST_STAT_IF, LUAU_AST_STAT_WHILE,
    LUAU_AST_STAT_REPEAT, LUAU_AST_STAT_BREAK, LUAU_AST_STAT_CONTINUE,
    LUAU_AST_STAT_RETURN, LUAU_AST_STAT_EXPR, LUAU_AST_STAT_LOCAL,
    LUAU_AST_STAT_FOR, LUAU_AST_STAT_FOR_IN, LUAU_AST_STAT_ASSIGN,
    LUAU_AST_STAT_COMPOUND_ASSIGN, LUAU_AST_STAT_FUNCTION,
    LUAU_AST_STAT_LOCAL_FUNCTION, LUAU_AST_STAT_TYPE_ALIAS,
    LUAU_AST_STAT_TYPE_FUNCTION, LUAU_AST_STAT_ERROR,
    // expressions
    LUAU_AST_EXPR_GROUP, LUAU_AST_EXPR_CONSTANT_NIL,
    LUAU_AST_EXPR_CONSTANT_BOOL, LUAU_AST_EXPR_CONSTANT_NUMBER,
    LUAU_AST_EXPR_CONSTANT_STRING, LUAU_AST_EXPR_LOCAL,
    LUAU_AST_EXPR_GLOBAL, LUAU_AST_EXPR_VARARGS, LUAU_AST_EXPR_CALL,
    LUAU_AST_EXPR_INDEX_NAME, LUAU_AST_EXPR_INDEX_EXPR,
    LUAU_AST_EXPR_FUNCTION, LUAU_AST_EXPR_TABLE, LUAU_AST_EXPR_UNARY,
    LUAU_AST_EXPR_BINARY, LUAU_AST_EXPR_TYPE_ASSERTION,
    LUAU_AST_EXPR_IF_ELSE, LUAU_AST_EXPR_INTERP_STRING,
    LUAU_AST_EXPR_CONSTANT_INTEGER, LUAU_AST_EXPR_ERROR,
    // types
    LUAU_AST_TYPE_REFERENCE, LUAU_AST_TYPE_TABLE, LUAU_AST_TYPE_FUNCTION,
    LUAU_AST_TYPE_TYPEOF, LUAU_AST_TYPE_UNION, LUAU_AST_TYPE_INTERSECTION,
    LUAU_AST_TYPE_OPTIONAL, LUAU_AST_TYPE_GROUP, LUAU_AST_TYPE_ERROR,
    LUAU_AST_TYPE_SINGLETON_STRING,
} LuauAstKind;

int luau_ast_node_count(const LuauParseResult* r);
int luau_ast_node_kind(const LuauParseResult* r, int i);
int luau_ast_node_parent(const LuauParseResult* r, int i);
LuauPosition luau_ast_node_begin(const LuauParseResult* r, int i);
LuauPosition luau_ast_node_end(const LuauParseResult* r, int i);
const char* luau_ast_node_string(const LuauParseResult* r, int i);
double luau_ast_node_number(const LuauParseResult* r, int i);
long long luau_ast_node_integer(const LuauParseResult* r, int i);
int luau_ast_node_boolean(const LuauParseResult* r, int i);

// ---- typed field accessors ---------------------------------------------------
//
// Given a flat node index, these return the named child node indices (or -1 when
// the role is absent / the node is the wrong kind) and scalar fields per node
// kind. Child indices refer back into the flat node list. Calling an accessor on
// a node of the wrong kind safely returns the empty/-1/0 sentinel.

// AstExprBinary: op (int), left (node), right (node).
int luau_ast_binary_op(const LuauParseResult* r, int i);
int luau_ast_binary_left(const LuauParseResult* r, int i);
int luau_ast_binary_right(const LuauParseResult* r, int i);

// AstExprUnary: op (int), operand (node).
int luau_ast_unary_op(const LuauParseResult* r, int i);
int luau_ast_unary_operand(const LuauParseResult* r, int i);

// AstExprGroup: expr (node).
int luau_ast_group_expr(const LuauParseResult* r, int i);

// AstExprCall: func (node), self (bool), arg count + arg(j) (node).
int luau_ast_call_func(const LuauParseResult* r, int i);
int luau_ast_call_self(const LuauParseResult* r, int i);
int luau_ast_call_arg_count(const LuauParseResult* r, int i);
int luau_ast_call_arg(const LuauParseResult* r, int i, int j);

// AstExprIndexName: expr (node), index name (string).
int luau_ast_index_name_expr(const LuauParseResult* r, int i);
const char* luau_ast_index_name_index(const LuauParseResult* r, int i);

// AstExprIndexExpr: expr (node), index (node).
int luau_ast_index_expr_expr(const LuauParseResult* r, int i);
int luau_ast_index_expr_index(const LuauParseResult* r, int i);

// AstExprFunction: param count + param name(j), vararg (bool), body (node).
int luau_ast_function_param_count(const LuauParseResult* r, int i);
const char* luau_ast_function_param_name(const LuauParseResult* r, int i, int j);
int luau_ast_function_vararg(const LuauParseResult* r, int i);
int luau_ast_function_body(const LuauParseResult* r, int i);

// AstExprTable: item count + item kind/key/value(j). kind: 0=list,1=record,2=general.
int luau_ast_table_item_count(const LuauParseResult* r, int i);
int luau_ast_table_item_kind(const LuauParseResult* r, int i, int j);
int luau_ast_table_item_key(const LuauParseResult* r, int i, int j);
int luau_ast_table_item_value(const LuauParseResult* r, int i, int j);

// AstExprTypeAssertion: expr (node), annotation (node).
int luau_ast_type_assertion_expr(const LuauParseResult* r, int i);
int luau_ast_type_assertion_annotation(const LuauParseResult* r, int i);

// AstExprIfElse: condition (node), trueexpr (node), falseexpr (node).
int luau_ast_ifelse_condition(const LuauParseResult* r, int i);
int luau_ast_ifelse_trueexpr(const LuauParseResult* r, int i);
int luau_ast_ifelse_falseexpr(const LuauParseResult* r, int i);

// AstExprInterpString: expr count + expr(j) (node).
int luau_ast_interp_expr_count(const LuauParseResult* r, int i);
int luau_ast_interp_expr(const LuauParseResult* r, int i, int j);

// AstStatBlock: stat count + stat(j) (node).
int luau_ast_block_stat_count(const LuauParseResult* r, int i);
int luau_ast_block_stat(const LuauParseResult* r, int i, int j);

// AstStatIf: condition (node), thenbody (node), elsebody (node).
int luau_ast_if_condition(const LuauParseResult* r, int i);
int luau_ast_if_thenbody(const LuauParseResult* r, int i);
int luau_ast_if_elsebody(const LuauParseResult* r, int i);

// AstStatWhile: condition (node), body (node).
int luau_ast_while_condition(const LuauParseResult* r, int i);
int luau_ast_while_body(const LuauParseResult* r, int i);

// AstStatRepeat: condition (node), body (node).
int luau_ast_repeat_condition(const LuauParseResult* r, int i);
int luau_ast_repeat_body(const LuauParseResult* r, int i);

// AstStatFor: var (name), from (node), to (node), step (node), body (node).
const char* luau_ast_for_var(const LuauParseResult* r, int i);
int luau_ast_for_from(const LuauParseResult* r, int i);
int luau_ast_for_to(const LuauParseResult* r, int i);
int luau_ast_for_step(const LuauParseResult* r, int i);
int luau_ast_for_body(const LuauParseResult* r, int i);

// AstStatForIn: var count + var(j) (name), value count + value(j) (node), body.
int luau_ast_forin_var_count(const LuauParseResult* r, int i);
const char* luau_ast_forin_var(const LuauParseResult* r, int i, int j);
int luau_ast_forin_value_count(const LuauParseResult* r, int i);
int luau_ast_forin_value(const LuauParseResult* r, int i, int j);
int luau_ast_forin_body(const LuauParseResult* r, int i);

// AstStatReturn: expr count + expr(j) (node).
int luau_ast_return_expr_count(const LuauParseResult* r, int i);
int luau_ast_return_expr(const LuauParseResult* r, int i, int j);

// AstStatExpr: expr (node).
int luau_ast_stat_expr_expr(const LuauParseResult* r, int i);

// AstStatLocal: var count + var name(j), value count + value(j) (node).
int luau_ast_local_var_count(const LuauParseResult* r, int i);
const char* luau_ast_local_var_name(const LuauParseResult* r, int i, int j);
int luau_ast_local_value_count(const LuauParseResult* r, int i);
int luau_ast_local_value(const LuauParseResult* r, int i, int j);

// AstStatAssign: lhs count + lhs(j) (node), rhs count + rhs(j) (node).
int luau_ast_assign_lhs_count(const LuauParseResult* r, int i);
int luau_ast_assign_lhs(const LuauParseResult* r, int i, int j);
int luau_ast_assign_rhs_count(const LuauParseResult* r, int i);
int luau_ast_assign_rhs(const LuauParseResult* r, int i, int j);

// AstStatCompoundAssign: op (int, AstExprBinary::Op), lhs (node), rhs (node).
int luau_ast_compound_op(const LuauParseResult* r, int i);
int luau_ast_compound_lhs(const LuauParseResult* r, int i);
int luau_ast_compound_rhs(const LuauParseResult* r, int i);

// AstStatFunction: name (node), func (node).
int luau_ast_stat_function_name(const LuauParseResult* r, int i);
int luau_ast_stat_function_func(const LuauParseResult* r, int i);

// AstStatLocalFunction: name (string), func (node).
const char* luau_ast_local_function_name(const LuauParseResult* r, int i);
int luau_ast_local_function_func(const LuauParseResult* r, int i);

// ---- type-annotation accessors -----------------------------------------------
//
// The type-annotation subtree (function/local annotations, type aliases, the
// types inside them) is flattened like the rest; these read its fields. Child
// indices refer back into the flat node list; wrong-kind calls return the
// empty/-1/0 sentinel.

// AstStatTypeAlias: name (string), exported (bool), aliased type (node),
// generic param count + generic(j) name (string).
const char* luau_ast_type_alias_name(const LuauParseResult* r, int i);
int luau_ast_type_alias_exported(const LuauParseResult* r, int i);
int luau_ast_type_alias_type(const LuauParseResult* r, int i);
int luau_ast_type_alias_generic_count(const LuauParseResult* r, int i);
const char* luau_ast_type_alias_generic_name(const LuauParseResult* r, int i, int j);

// AstTypeReference: prefix (string, "" if none), name (string), type-argument
// count + arg(j) type (node, or -1 if the argument is a type pack).
const char* luau_ast_type_reference_prefix(const LuauParseResult* r, int i);
const char* luau_ast_type_reference_name(const LuauParseResult* r, int i);
int luau_ast_type_reference_param_count(const LuauParseResult* r, int i);
int luau_ast_type_reference_param(const LuauParseResult* r, int i, int j);

// AstTypeUnion / AstTypeIntersection: member count + member(j) (node).
int luau_ast_type_union_count(const LuauParseResult* r, int i);
int luau_ast_type_union_member(const LuauParseResult* r, int i, int j);
int luau_ast_type_intersection_count(const LuauParseResult* r, int i);
int luau_ast_type_intersection_member(const LuauParseResult* r, int i, int j);

// AstTypeSingletonString: the string literal value.
const char* luau_ast_type_singleton_string_value(const LuauParseResult* r, int i);

// AstTypeTable: property count + prop(j) name (string), type (node), and access
// (0=read-write, 1=read-only, 2=write-only).
int luau_ast_type_table_prop_count(const LuauParseResult* r, int i);
const char* luau_ast_type_table_prop_name(const LuauParseResult* r, int i, int j);
int luau_ast_type_table_prop_type(const LuauParseResult* r, int i, int j);
int luau_ast_type_table_prop_access(const LuauParseResult* r, int i, int j);

// AstTypeTypeof: the expression inside `typeof(...)` (node).
int luau_ast_type_typeof_expr(const LuauParseResult* r, int i);
// AstTypeGroup: the parenthesized inner type (node).
int luau_ast_type_group_type(const LuauParseResult* r, int i);

// AstExprFunction parameter annotations: per-parameter annotation type (node, or
// -1 if the parameter is unannotated). Pairs with `luau_ast_function_param_*`.
int luau_ast_function_param_annotation(const LuauParseResult* r, int i, int j);

LUAU_END_DECLS
