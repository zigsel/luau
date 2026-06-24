//! Native code generation / JIT.
//!
//! Wraps both the small `luacodegen.h` C API and the richer `CodeGen.h` C++ API
//! (compilation stats, disassembly, native-execution toggles) via the shim.

const std = @import("std");
const c = @import("bindings");
const Lua = @import("lua.zig").Lua;

/// Whether the native code generator is supported on this platform/build.
pub fn supported() bool {
    return c.luau_codegen_supported() != 0;
}

/// Create a code-generator instance for the state. Check `supported()` first;
/// call once after `Lua.init`, before loading code you intend to JIT.
pub fn create(lua: *Lua) void {
    c.luau_codegen_create(lua.toRaw());
}

/// Natively compile the function at `idx` (and all its inner functions).
pub fn compile(lua: *Lua, idx: i32) void {
    c.luau_codegen_compile(lua.toRaw(), idx);
}

// ---- richer CodeGen.h surface (via the C++ shim) ---------------------------

/// Result code of a native compilation, mirroring `CodeGenCompilationResult`.
pub const CompilationResult = enum(c_int) {
    success = 0,
    nothing_to_compile = 1,
    not_native_module = 2,
    not_initialized = 3,
    overflow_instruction_limit = 4,
    overflow_block_limit = 5,
    overflow_block_instruction_limit = 6,
    assembler_finalization_failure = 7,
    lowering_failure = 8,
    allocation_failed = 9,
    _,
};

/// Statistics from a native compilation.
pub const Stats = struct {
    bytecode_size: usize,
    native_code_size: usize,
    native_data_size: usize,
    native_metadata_size: usize,
    functions_total: u32,
    functions_compiled: u32,
    functions_bound: u32,
};

/// Compile the function at `idx`, returning the result code and (optionally)
/// filling `stats`.
pub fn compileWithStats(lua: *Lua, idx: i32, flags: u32, stats: ?*Stats) CompilationResult {
    var s: c.LuauCodegenStats = undefined;
    const r = c.luau_codegen_compile2(lua.toRaw(), idx, flags, &s);
    if (stats) |out| out.* = .{
        .bytecode_size = s.bytecode_size,
        .native_code_size = s.native_code_size,
        .native_data_size = s.native_data_size,
        .native_metadata_size = s.native_metadata_size,
        .functions_total = s.functions_total,
        .functions_compiled = s.functions_compiled,
        .functions_bound = s.functions_bound,
    };
    return @enumFromInt(r);
}

/// Enable/disable native execution globally for the VM.
pub fn setNativeExecutionEnabled(lua: *Lua, enabled: bool) void {
    c.luau_codegen_set_native_execution_enabled(lua.toRaw(), @intFromBool(enabled));
}

/// Disable native execution for the function at stack `level`.
pub fn disableNativeForFunction(lua: *Lua, level: i32) void {
    c.luau_codegen_disable_native_for_function(lua.toRaw(), level);
}

/// Disassemble the function at `idx` to text, owned by `allocator`.
pub fn getAssembly(allocator: std.mem.Allocator, lua: *Lua, idx: i32, include_assembly: bool, include_ir: bool) ![]u8 {
    const raw = c.luau_codegen_get_assembly(lua.toRaw(), idx, @intFromBool(include_assembly), @intFromBool(include_ir));
    if (raw == null) return error.DisassemblyFailed;
    defer std.c.free(raw);
    return allocator.dupe(u8, std.mem.span(raw));
}
