//! Route the C++ tooling's `operator new`/`delete` (STL containers + Luau's AST
//! arena — effectively all tooling allocation) through a Zig allocator.
//!
//! This is PROCESS-GLOBAL: there is one C++ `operator new`, hence one allocator.
//! Call `setAllocator` once, early, before heavy tooling use; until then a libc
//! fallback is used. The VM heap is separate and per-state (see `Lua.init`).

const std = @import("std");
const c = @import("bindings");

var active: std.mem.Allocator = undefined;

fn zalloc(_: ?*anyopaque, size: usize, alignment: usize) callconv(.c) ?*anyopaque {
    const a = std.mem.Alignment.fromByteUnits(alignment);
    const p = active.vtable.alloc(active.ptr, size, a, @returnAddress()) orelse return null;
    return @ptrCast(p);
}
fn zfree(_: ?*anyopaque, ptr: ?*anyopaque, size: usize, alignment: usize) callconv(.c) void {
    const a = std.mem.Alignment.fromByteUnits(alignment);
    const buf: [*]u8 = @ptrCast(ptr.?);
    active.vtable.free(active.ptr, buf[0..size], a, @returnAddress());
}

/// Install `a` as the global tooling allocator. `a` (and the state it points at)
/// must outlive every tooling call; call `useDefaultAllocator` before tearing it
/// down. Idempotent; safe to switch at runtime (existing blocks remember their
/// allocator).
pub fn setAllocator(a: std.mem.Allocator) void {
    active = a;
    c.luau_set_allocator(zalloc, zfree, null);
}

/// Revert tooling allocation to the libc fallback.
pub fn useDefaultAllocator() void {
    c.luau_set_allocator(null, null, null);
}
