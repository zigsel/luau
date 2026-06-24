// Route the C++ tooling's `operator new`/`delete` (STL containers + Luau's AST
// arena — effectively all tooling allocation) through a host-supplied allocator.
//
// This is PROCESS-GLOBAL: there is one `operator new`, so one allocator. Set it
// once, early, before heavy tooling use. Until set, a libc fallback is used.
// (The VM's own heap is separate — it uses the per-state allocator from
// `Lua.init`. And the shim's small returned-string buffers stay on libc malloc,
// so `free`/`malloc` are intentionally NOT overridden.)
#pragma once

#include "common.h"

LUAU_BEGIN_DECLS

// `alloc(ctx, size, align)` returns `size` bytes aligned to `align` (or NULL).
// `free(ctx, ptr, size, align)` releases a block from `alloc` with the same args.
typedef void* (*LuauAllocFn)(void* ctx, size_t size, size_t align);
typedef void (*LuauFreeFn)(void* ctx, void* ptr, size_t size, size_t align);

// Install the global tooling allocator. Pass (NULL, NULL, NULL) to revert to the
// libc fallback. Blocks already allocated remember how to free themselves, so
// switching at runtime is safe.
void luau_set_allocator(LuauAllocFn alloc, LuauFreeFn free, void* ctx);

LUAU_END_DECLS
