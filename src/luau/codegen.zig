//! Native code generation / JIT for Luau — one namespace for the whole surface.
//!
//!   * high-level (the common case): turn native execution on, `jit`-compile a
//!     loaded function with stats, inspect the disassembly — the VM runs native.
//!   * low-level construction kit (exotic): hand-build machine code / IR with the
//!     `x64`, `a64`, and `ir` submodules.

const native = @import("../lua/codegen.zig");

// ---- high-level JIT (lua C API + CodeGen.h) ---------------------------------
pub const supported = native.supported;
pub const create = native.create;
/// Native-compile a loaded function (the JIT). Named `jit` to avoid confusion
/// with `luau.compile` (source → bytecode).
pub const jit = native.compile;
pub const CompilationResult = native.CompilationResult;
pub const Stats = native.Stats;
pub const compileWithStats = native.compileWithStats;
pub const setNativeExecutionEnabled = native.setNativeExecutionEnabled;
pub const disableNativeForFunction = native.disableNativeForFunction;
pub const getAssembly = native.getAssembly;

// ---- low-level construction kit (via the C++ shims) ------------------------
/// The x64 assembler — every instruction, with heap-handle operands
/// (`AssemblyBuilderX64`).
pub const x64 = @import("codegen/x64.zig");
/// The AArch64 assembler (`AssemblyBuilderA64`).
pub const a64 = @import("codegen/a64.zig");
/// Construct and inspect JIT IR (`IrBuilder`, the Proto-free surface).
pub const ir = @import("codegen/ir.zig");
