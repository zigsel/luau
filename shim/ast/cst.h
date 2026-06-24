// Shim: Luau CST (Concrete Syntax Tree) — the lossless parse view a FORMATTER
// or codemod needs. Parsing with `storeCstData` + `captureComments` enabled
// retains comment trivia and, for every AST node, an associated `CstNode`
// carrying the *exact* token positions and separators the lossy AST drops
// (commas, equals signs, keyword positions, quote styles, etc).
//
// This shim exposes a SOLID, formatter-oriented core:
//   * the comment list (type + source location + text), and the hotcomments;
//   * a flattened, indexable node view (mirrors ast.cpp's walker) where each
//     node additionally reports whether it has a CstNode and that CstNode's
//     subclass kind;
//   * for each node, a flattened list of named "trivia positions" — every
//     scalar `Position` field a CstNode subclass exposes (separators, keyword
//     positions, brackets, quote info). Each entry is (name, line, column),
//     where a "missing" Position (line==UINT_MAX) is reported via a flag.
//
// OMITTED (kept out to bound the surface; add later if a formatter needs them):
//   * CstNode fields that are themselves nested nodes/arrays-of-nodes
//     (e.g. CstExprFunction::attrLists, CstTypeTable::Item::stringInfo);
//   * the raw `AstArray<char>` source-string buffers on numeric/string CST
//     nodes (the AST already exposes the decoded value) — but the *quote
//     style* and *block depth* ARE surfaced as integer trivia values;
//   * structured per-table-item separators are flattened into the position
//     list as repeated "item.separator"/"item.equals" entries in order.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Parse `src` WITH CST capture enabled. Always returns a non-null handle;
// inspect errors with the accessors below. Free with `luau_cst_free`.
LuauCst* luau_cst_parse(const char* src, size_t len);
void luau_cst_free(LuauCst* c);

// ---- diagnostics -------------------------------------------------------------

int luau_cst_error_count(const LuauCst* c);
const char* luau_cst_error_message(const LuauCst* c, int i);
LuauPosition luau_cst_error_position(const LuauCst* c, int i);
int luau_cst_has_root(const LuauCst* c);
size_t luau_cst_line_count(const LuauCst* c);

// ---- comments ----------------------------------------------------------------
//
// Comment trivia, in source order. A formatter re-emits these verbatim; the
// position spans tell it where each comment sits relative to nodes.

typedef enum LuauCstCommentKind {
    LUAU_CST_COMMENT_LINE = 0,  // -- ...
    LUAU_CST_COMMENT_BLOCK = 1, // --[[ ... ]]
    LUAU_CST_COMMENT_BROKEN = 2,
} LuauCstCommentKind;

int luau_cst_comment_count(const LuauCst* c);
int luau_cst_comment_kind(const LuauCst* c, int i);
LuauPosition luau_cst_comment_begin(const LuauCst* c, int i);
LuauPosition luau_cst_comment_end(const LuauCst* c, int i);
// The comment text sliced out of the source (between the span). Empty if the
// span is degenerate. Returns a stable pointer owned by the handle.
const char* luau_cst_comment_text(const LuauCst* c, int i);

// Hotcomments (`--!strict`, etc): content with the leading `--!` stripped.
int luau_cst_hotcomment_count(const LuauCst* c);
const char* luau_cst_hotcomment_content(const LuauCst* c, int i);

// ---- flattened node view -----------------------------------------------------
//
// Same depth-first flattening as the AST shim. `kind` reuses LuauAstKind values
// from ast.h, so callers can share the enum.

int luau_cst_node_count(const LuauCst* c);
int luau_cst_node_kind(const LuauCst* c, int i);   // LuauAstKind
int luau_cst_node_parent(const LuauCst* c, int i); // -1 for root
LuauPosition luau_cst_node_begin(const LuauCst* c, int i);
LuauPosition luau_cst_node_end(const LuauCst* c, int i);

// Whether this AST node has an associated CstNode in the cstNodeMap.
int luau_cst_node_has_cst(const LuauCst* c, int i);

// The CstNode subclass for this node (LuauCstKind below), or LUAU_CST_NONE.
typedef enum LuauCstKind {
    LUAU_CST_NONE = 0,
    LUAU_CST_ATTR, LUAU_CST_PARAMETRIZED_ATTR,
    LUAU_CST_EXPR_GROUP, LUAU_CST_EXPR_CONSTANT_NUMBER,
    LUAU_CST_EXPR_CONSTANT_INTEGER, LUAU_CST_EXPR_CONSTANT_STRING,
    LUAU_CST_EXPR_CALL, LUAU_CST_EXPR_INDEX_EXPR, LUAU_CST_EXPR_FUNCTION,
    LUAU_CST_EXPR_TABLE, LUAU_CST_EXPR_OP, LUAU_CST_EXPR_TYPE_ASSERTION,
    LUAU_CST_EXPR_IF_ELSE, LUAU_CST_EXPR_INTERP_STRING,
    LUAU_CST_STAT_DO, LUAU_CST_STAT_REPEAT, LUAU_CST_STAT_RETURN,
    LUAU_CST_STAT_LOCAL, LUAU_CST_STAT_FOR, LUAU_CST_STAT_FOR_IN,
    LUAU_CST_STAT_ASSIGN, LUAU_CST_STAT_COMPOUND_ASSIGN,
    LUAU_CST_STAT_FUNCTION, LUAU_CST_STAT_LOCAL_FUNCTION,
    LUAU_CST_STAT_TYPE_ALIAS, LUAU_CST_STAT_TYPE_FUNCTION,
    LUAU_CST_GENERIC_TYPE, LUAU_CST_GENERIC_TYPE_PACK,
    LUAU_CST_TYPE_REFERENCE, LUAU_CST_TYPE_TABLE, LUAU_CST_TYPE_FUNCTION,
    LUAU_CST_TYPE_TYPEOF, LUAU_CST_TYPE_UNION, LUAU_CST_TYPE_INTERSECTION,
    LUAU_CST_TYPE_SINGLETON_STRING, LUAU_CST_TYPE_GROUP,
    LUAU_CST_TYPE_PACK_EXPLICIT, LUAU_CST_TYPE_PACK_GENERIC,
    LUAU_CST_OTHER, // a CstNode we do not yet decode trivia for
} LuauCstKind;

int luau_cst_node_cst_kind(const LuauCst* c, int i); // LuauCstKind

// ---- per-node trivia positions ----------------------------------------------
//
// Each node with a decoded CstNode exposes an ordered list of named trivia
// entries: token positions (commas, `=`, keywords, brackets), and a few
// integer "info" values (quote style, block depth, separator kind) folded in
// as entries whose position is degenerate and whose `value` is meaningful.

// Number of trivia entries for node `i`.
int luau_cst_node_trivia_count(const LuauCst* c, int i);
// Stable name of trivia entry `j` of node `i` (e.g. "equals", "comma",
// "functionKeyword", "open", "close", "quoteStyle"). Owned by the handle.
const char* luau_cst_node_trivia_name(const LuauCst* c, int i, int j);
// Source position of trivia entry `j`. Meaningless when `trivia_missing` is 1.
LuauPosition luau_cst_node_trivia_position(const LuauCst* c, int i, int j);
// 1 if the entry is a "missing" Position (Position::missing(), absent token).
int luau_cst_node_trivia_missing(const LuauCst* c, int i, int j);
// For integer "info" entries (quoteStyle / blockDepth / separator kind), the
// value. 0 for ordinary positional entries.
long long luau_cst_node_trivia_value(const LuauCst* c, int i, int j);

LUAU_END_DECLS
