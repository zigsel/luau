// Shim: Luau Analysis require-path suggestion DATA structures.
//
// The full require-suggestion machinery (RequireSuggester / RequireNode /
// FileResolver::getRequireSuggestions) is abstract and demands a live host
// resolver that walks a project tree, so it is NOT bound here (see the slice
// manifest's "excluded" list). What IS host-free are the plain data carriers
// the suggestion API hands back to an editor: `RequireSuggestion` (a single
// autocomplete target: label + full require path + tags) and `RequireAlias`
// (a `.luaurc`-style alias name + tags). This shim provides a faithful
// round-trip over those two POD structs so a caller can build, carry, and
// inspect require-path suggestions independently of any resolver.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// -- RequireSuggestion: one editor autocomplete target. --

LUAU_HANDLE(LuauRequireSuggestion);

// Build a RequireSuggestion from `label`/`fullPath` (each a counted byte span,
// need not be NUL-terminated). Tags start empty; append with _add_tag. Always
// returns a handle; free with luau_analysis_requiresuggestion_free.
LuauRequireSuggestion* luau_analysis_requiresuggestion_new(
    const char* label, size_t label_len, const char* full_path, size_t full_path_len);

// Append a tag (counted byte span).
void luau_analysis_requiresuggestion_add_tag(
    LuauRequireSuggestion* h, const char* tag, size_t tag_len);

// The user-facing label (NUL-terminated, borrows the handle's storage).
const char* luau_analysis_requiresuggestion_label(const LuauRequireSuggestion* h);
// The full require path that would be inserted (NUL-terminated, borrowed).
const char* luau_analysis_requiresuggestion_full_path(const LuauRequireSuggestion* h);
// Number of tags attached.
int luau_analysis_requiresuggestion_tag_count(const LuauRequireSuggestion* h);
// Tag `i` (NUL-terminated, borrowed), or "" if out of range.
const char* luau_analysis_requiresuggestion_tag(const LuauRequireSuggestion* h, int i);

void luau_analysis_requiresuggestion_free(LuauRequireSuggestion* h);

// -- RequireAlias: a `.luaurc` alias visible to a require node. --

LUAU_HANDLE(LuauRequireAlias);

// Build a RequireAlias from the unprefixed alias `name` (no leading `@`), a
// counted byte span. Tags start empty. Free with luau_analysis_requirealias_free.
LuauRequireAlias* luau_analysis_requirealias_new(const char* name, size_t name_len);

// Append a tag (counted byte span).
void luau_analysis_requirealias_add_tag(
    LuauRequireAlias* h, const char* tag, size_t tag_len);

// The unprefixed alias name (NUL-terminated, borrowed).
const char* luau_analysis_requirealias_name(const LuauRequireAlias* h);
// Number of tags attached.
int luau_analysis_requirealias_tag_count(const LuauRequireAlias* h);
// Tag `i` (NUL-terminated, borrowed), or "" if out of range.
const char* luau_analysis_requirealias_tag(const LuauRequireAlias* h, int i);

void luau_analysis_requirealias_free(LuauRequireAlias* h);

LUAU_END_DECLS
