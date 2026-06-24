// Shim: function ATTRIBUTES (AstAttr: @native/@checked/@deprecated/...) and the
// CONFUSABLES lookup (Ast module).
//
// Attributes: Luau functions may carry attributes (`@native`, `@checked`,
// `@deprecated`, `@debug_noinline`). This shim parses source, walks every
// function node (AstExprFunction and AstStatFunction/LocalFunction), and exposes
// each function's attribute list as a flat (functionIndex, attrType) table.
//
// Confusables: a single bounded lookup over Luau::findConfusable — given a
// Unicode codepoint, is it a known visually-confusable character, and what is
// the suggested replacement.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Mirrors Luau::AstAttr::Type.
typedef enum LuauAttrType {
    LUAU_ATTR_CHECKED = 0,
    LUAU_ATTR_NATIVE,
    LUAU_ATTR_DEPRECATED,
    LUAU_ATTR_DEBUG_NOINLINE,
    LUAU_ATTR_UNKNOWN,
} LuauAttrType;

// Parse `src` and collect the attributes of every function in it. Always returns
// a non-null handle. Free with `luau_attributes_free`.
LuauAttributes* luau_attributes_parse(const char* src, size_t len);

// 1 if the source parsed cleanly; attributes are still collected on a partial
// parse where possible.
int luau_attributes_parsed_ok(const LuauAttributes* a);

// Number of attribute occurrences collected (across all functions).
int luau_attributes_count(const LuauAttributes* a);

// The i-th attribute's type (a LuauAttrType), or -1 if out of range.
int luau_attributes_type(const LuauAttributes* a, int i);

// A stable per-function id for the i-th attribute, so callers can group the
// attributes that belong to the same function. -1 if out of range.
int luau_attributes_function(const LuauAttributes* a, int i);

// The begin position of the i-th attribute. {0,0} if out of range.
LuauPosition luau_attributes_position(const LuauAttributes* a, int i);

void luau_attributes_free(LuauAttributes* a);

// ---- confusables -------------------------------------------------------------

// If `codepoint` is a known confusable, returns its suggested replacement
// (borrowed static string); otherwise NULL.
const char* luau_confusable_suggestion(unsigned int codepoint);

// 1 if `codepoint` is a known confusable, else 0.
int luau_confusable_is(unsigned int codepoint);

LUAU_END_DECLS
