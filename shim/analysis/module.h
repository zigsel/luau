// extern "C" shim for MODULE / SCOPE / DEFINITION inspection of a checked Luau
// module.
//
// `luau_module_check` typechecks a single in-memory module (full type graph
// retained) and keeps the Frontend + Module alive in an opaque handle. From it
// you can read module-level metadata, the module's return type, the top-level
// scope's bindings (name + inferred TypeId + declaration Location), and the
// module's exported type bindings (name + TypeId).
//
// The `LuauType*` / `LuauTypePack*` handles returned here are the SAME opaque
// handles produced by the `types` shim, so every accessor in "types.h" works on
// them. They are owned by the `LuauModule*` and stay valid until it is freed
// with `luau_module_free`; do NOT free them individually.

#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// A source span (0-based), used for declaration locations.
typedef struct LuauModuleLocation {
    unsigned int begin_line;
    unsigned int begin_column;
    unsigned int end_line;
    unsigned int end_column;
} LuauModuleLocation;

// ---- checker ---------------------------------------------------------------

// Typecheck the module "main" with `retainFullTypeGraphs = true` and builtin
// globals registered. Never returns NULL (errors are reported below). Keeps the
// Frontend + checked Module alive.
LuauModule* luau_module_check(const char* src, size_t len);

void luau_module_free(LuauModule* h);

// ---- module-level info -----------------------------------------------------

// The module's name (malloc'd, caller frees), or NULL if unavailable.
char* luau_module_name(const LuauModule* h);
// The module's human-readable name (malloc'd, caller frees), or NULL.
char* luau_module_human_name(const LuauModule* h);

// 1 if the module type-checked (a Module exists), else 0.
int luau_module_checked(const LuauModule* h);

// Number of type errors reported during checking.
int luau_module_error_count(const LuauModule* h);
// The i-th error message (borrowed; "" if out of range).
const char* luau_module_error_message(const LuauModule* h, int i);
// The i-th error's start position.
LuauPosition luau_module_error_position(const LuauModule* h, int i);

// 1 if the check timed out / was cancelled (from Module flags), else 0.
int luau_module_timed_out(const LuauModule* h);
int luau_module_cancelled(const LuauModule* h);

// ---- module return type ----------------------------------------------------

// The module's return type pack (Module::returnType), or NULL if none.
LuauTypePack* luau_module_return_type(LuauModule* h);

// ---- top-level scope bindings ----------------------------------------------
//
// Enumerates the bindings of the module's top-level scope (Scope::bindings).
// Iteration order is the underlying unordered_map order and is stable for a
// given handle.

// Number of bindings in the module's top-level scope (0 if none).
int luau_module_binding_count(LuauModule* h);
// The i-th binding's source name (borrowed; NULL if out of range).
const char* luau_module_binding_name(LuauModule* h, int i);
// The i-th binding's inferred type (NULL if out of range / no type).
LuauType* luau_module_binding_type(LuauModule* h, int i);
// The i-th binding's declaration location.
LuauModuleLocation luau_module_binding_location(LuauModule* h, int i);
// 1 if the i-th binding is marked deprecated, else 0.
int luau_module_binding_deprecated(LuauModule* h, int i);

// Convenience: inferred type of the top-level binding named `name`, or NULL.
LuauType* luau_module_binding_lookup(LuauModule* h, const char* name);

// ---- exported types --------------------------------------------------------
//
// Enumerates Module::exportedTypeBindings (the module's exported type aliases).

// Number of exported type bindings (0 if none).
int luau_module_exported_type_count(LuauModule* h);
// The i-th exported type's name (borrowed; NULL if out of range).
const char* luau_module_exported_type_name(LuauModule* h, int i);
// The i-th exported type's underlying TypeId (NULL if out of range).
// WARNING: this is the alias' underlying type and is only meaningful directly
// when the alias has no type parameters.
LuauType* luau_module_exported_type(LuauModule* h, int i);

LUAU_END_DECLS
