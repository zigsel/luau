// extern "C" shim for the Luau LINTER (Config + Analysis modules).
//
// Two capabilities:
//
//  1. RULE ENUMERATION — the full set of lint rules is a compile-time enum
//     (`LintWarning::Code`, 0..Code__Count). `luau_lint_rule_count` returns the
//     count; `luau_lint_rule_name(i)` / `luau_lint_rule_code(i)` give the
//     human-readable name (`LintWarning::getName`) and numeric code for rule `i`.
//
//  2. STANDALONE LINT — `luau_lint_source` lints a source string with full rule
//     control: `enabled_mask` selects which rules fire (bit `i` => code `i`),
//     `fatal_mask` selects which fired warnings are classified as errors. The
//     masks mirror `LintOptions::warningMask`. The result is an opaque handle
//     holding the collected warnings (code, name, message, position); query it
//     with the accessors below, then free it.
//
// Internally this drives a single in-memory `Frontend` with
// `FrontendOptions::enabledLintWarnings` set to `enabled_mask` and the config
// resolver's `fatalLint` set to `fatal_mask`, so both "enabled" and "fatal"
// are honored exactly as the upstream pipeline would.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// ---- rule enumeration (compile-time; no source needed) ---------------------

// Number of lint rules (== LintWarning::Code__Count).
int luau_lint_rule_count(void);
// Name of rule `i` (e.g. "LocalShadow"); "" if `i` is out of range.
const char* luau_lint_rule_name(int i);
// Numeric LintWarning::Code of rule `i`; -1 if `i` is out of range.
int luau_lint_rule_code(int i);

// ---- standalone lint -------------------------------------------------------

// Lint `src` (length `len`). `enabled_mask` bit `i` enables rule with code `i`;
// `fatal_mask` bit `i` marks that rule's warnings as fatal (reported as errors).
// Always returns a handle (empty on internal failure); call luau_lint_free.
LuauLint* luau_lint_source(const char* src, size_t len, unsigned long long enabled_mask, unsigned long long fatal_mask);

// Number of collected warnings (errors + warnings).
int luau_lint_count(const LuauLint* h);
// LintWarning::Code of warning `i`; -1 out of range.
int luau_lint_warning_code(const LuauLint* h, int i);
// Rule name of warning `i`; "" out of range.
const char* luau_lint_warning_name(const LuauLint* h, int i);
// Message text of warning `i`; "" out of range.
const char* luau_lint_warning_message(const LuauLint* h, int i);
// Start position of warning `i`; {0,0} out of range.
LuauPosition luau_lint_warning_position(const LuauLint* h, int i);
// 1 if warning `i` was classified as fatal (an error), else 0.
int luau_lint_warning_fatal(const LuauLint* h, int i);

void luau_lint_free(LuauLint* h);

LUAU_END_DECLS
