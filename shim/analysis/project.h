// Shim: multi-module Luau type checking with require() resolution.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// A "project": a set of named in-memory modules where one may require another.
LuauProject* luau_project_new(void);

// Register (or replace) a module's source under `name`.
void luau_project_add_module(LuauProject* p, const char* name, size_t name_len, const char* src, size_t src_len);

// Type-check `entry_name` and every module it (transitively) requires.
// Collected errors span all checked modules; query them with the accessors below.
void luau_project_check(LuauProject* p, const char* entry_name);

// Errors collected by the last `luau_project_check`, across all checked modules.
int luau_project_error_count(const LuauProject* p);
const char* luau_project_error_module_name(const LuauProject* p, int i);
const char* luau_project_error_message(const LuauProject* p, int i);
LuauPosition luau_project_error_position(const LuauProject* p, int i);

void luau_project_free(LuauProject* p);

LUAU_END_DECLS
