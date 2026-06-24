// Shim: Luau Config — `.luaurc` parsing.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

typedef enum LuauConfigMode {
    LUAU_MODE_NOCHECK = 0,
    LUAU_MODE_NONSTRICT = 1,
    LUAU_MODE_STRICT = 2,
    LUAU_MODE_DEFINITION = 3,
} LuauConfigMode;

// Parse `.luaurc` JSON `contents`; always returns a handle. Free with free().
LuauConfig* luau_config_parse(const char* contents, size_t len);
// NUL-terminated parse error, or NULL if parsing succeeded.
const char* luau_config_error(const LuauConfig* h);
// The type-checking mode (see LuauConfigMode).
int luau_config_mode(const LuauConfig* h);
// Declared globals.
int luau_config_global_count(const LuauConfig* h);
const char* luau_config_global(const LuauConfig* h, int i);
// Whether lint/type issues are configured to be errors.
int luau_config_lint_errors(const LuauConfig* h);
int luau_config_type_errors(const LuauConfig* h);

// Aliases declared in `aliases` (e.g. import path aliases). Names are returned
// in their original case; values are the alias targets.
int luau_config_alias_count(const LuauConfig* h);
const char* luau_config_alias_name(const LuauConfig* h, int i);     // original case
const char* luau_config_alias_value(const LuauConfig* h, int i);
const char* luau_config_alias_key(const LuauConfig* h, int i);      // case-folded key
const char* luau_config_alias_location(const LuauConfig* h, int i); // .luaurc location
// Case-insensitively resolve an alias name to its target value, or NULL if none.
const char* luau_config_alias_resolve(const LuauConfig* h, const char* name);

// Whether `name` is a syntactically valid alias name.
int luau_config_is_valid_alias(const char* name);

// Lint rules. The count is the number of LintWarning::Code values (Code__Count).
// Index `i` corresponds to LintWarning::Code value `i`.
int luau_config_lint_rule_count(void);
// Rule name for code `i` (LintWarning::getName), or "" if out of range.
const char* luau_config_lint_rule_name(int i);
// Whether code `i` is enabled / fatal in this config (Config::enabledLint /
// Config::fatalLint). Returns 0 if `i` is out of range.
int luau_config_lint_rule_enabled(const LuauConfig* h, int i);
int luau_config_lint_rule_fatal(const LuauConfig* h, int i);

void luau_config_free(LuauConfig* h);

LUAU_END_DECLS
