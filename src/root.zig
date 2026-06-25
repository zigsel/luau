//! luau — complete, idiomatic Zig bindings to the Luau language.
//!
//! The public surface is pure Zig; the raw `translate-c` layer is internal
//! plumbing and never exposed. Two tiers:
//!
//!   * **Embedding** the VM — `Lua` plus the value/table/ref/buffer helpers, the
//!     compiler and the native JIT. (the inherited Lua C API, made idiomatic)
//!   * **Tooling** over Luau source — `ast`, `analysis`, `config`, `require`,
//!     `bytecode`. (Luau's own additions, via extern "C" shims)

const build_options = @import("config");

const lua_mod = @import("lua/lua.zig");
const errors = @import("lua/errors.zig");
const enums = @import("lua/enums.zig");
const value = @import("lua/value.zig");

// ---- the Lua state -----------------------------------------------------------

/// The Luau state / thread (a zero-cost view over `lua_State*`).
pub const Lua = lua_mod.Lua;
/// A saved stack height; `restore()` trims back to it.
pub const Scope = lua_mod.Scope;

// ---- value handles -----------------------------------------------------------

/// A typed, registry-pinned reference to a Luau value.
pub const Ref = @import("lua/ref.zig").Ref;
/// An untyped registry reference (re-push only).
pub const AnyRef = @import("lua/ref.zig").AnyRef;
/// An ergonomic handle to a Luau table (get/set/len/iterate).
pub const Table = @import("lua/table.zig").Table;
/// A Luau string builder.
pub const Buffer = @import("lua/buffer.zig").Buffer;

// ---- enums, errors and scalars ----------------------------------------------

pub const Error = errors.Error;
pub const Status = errors.Status;
pub const CoStatus = errors.CoStatus;
pub const ResumeStatus = lua_mod.Lua.ResumeStatus;
/// Error returned when a stack value can't be marshalled to the requested type.
pub const MarshalError = value.Error;

pub const LuaType = enums.LuaType;
pub const GcAction = enums.GcAction;
pub const registry_index = enums.registry_index;
pub const globals_index = enums.globals_index;
pub const environ_index = enums.environ_index;
pub const multret = enums.multret;
pub const noref = enums.noref;
pub const refnil = enums.refnil;

pub const Number = lua_mod.Number;
pub const Integer = lua_mod.Integer;
pub const Unsigned = lua_mod.Unsigned;
pub const Vector = lua_mod.Vector;

/// Raw `extern "C"` callback typedefs — the escape hatches for low-level interop
/// (registering a raw `lua_CFunction`, the userdata fast-paths). Kept out of the
/// top-level surface so it stays pure idiomatic Zig; reach for these only when a
/// helper like `setFn`/`registerType` doesn't fit.
pub const raw = struct {
    const misc = @import("lua/misc.zig");
    pub const CFn = lua_mod.CFn;
    pub const Continuation = lua_mod.Continuation;
    pub const Destructor = misc.Destructor;
    pub const UserdataDirectAccess = misc.UserdataDirectAccess;
    pub const UserdataDirectNamecall = misc.UserdataDirectNamecall;
    pub const UserdataDirectFieldGet = misc.UserdataDirectFieldGet;
    pub const CompileConstant = @import("lua/compiler.zig").CompileConstant;
};

// ---- compiler & native codegen ----------------------------------------------

/// The Luau compiler: source/AST → bytecode, options, compile constants, and
/// structured errors (`compiler.errors`).
pub const compiler = @import("luau/compiler.zig");
pub const CompileOptions = compiler.CompileOptions;
/// Compile Luau source to owned bytecode.
pub const compile = compiler.compile;
/// Native code generation / JIT: high-level (`supported`/`create`/
/// `compileWithStats`/disassembly/toggles) plus the low-level `assembler`,
/// `asm_x64`, `asm_a64`, and `ir` construction kit.
pub const codegen = @import("luau/codegen.zig");

// ---- Luau language tooling (via extern "C" shims) ---------------------------

/// Parse Luau source + full AST: node walking, typed accessors, the `builder`
/// (construct & compile an AST), `prettyprint`/`format`, `lexer`, `cst`,
/// `attributes`.
pub const ast = @import("luau/ast.zig");
/// Type-check, lint and autocomplete; plus the type graph (`types`),
/// `query`/`locate` (hover & go-to-definition), `builtins` (host type defs),
/// `relations`/`normalize`/`transforms`, `project` (multi-module), `viz`, …
pub const analysis = @import("luau/analysis.zig");
/// Parse `.luaurc` (mode, globals, lint settings).
pub const config = @import("luau/config.zig");
/// Require-by-string: install a configurable `require` global.
pub const require = @import("luau/require.zig");
/// Hand-emit Luau bytecode (`BytecodeBuilder`) + the bytecode graph.
pub const bytecode = @import("luau/bytecode.zig");

/// Project Zig types into Luau type definitions (so `analysis`/`luau-lsp`
/// understand your host API). `declsFor(T, name)` builds a `declare` block.
pub const luauType = @import("luau/declare.zig").luauType;
pub const declsFor = @import("luau/declare.zig").declsFor;

/// Route the C++ tooling's `operator new`/`delete` through a Zig allocator
/// (process-global; the VM heap stays per-state via `Lua.init`).
pub const setAllocator = @import("luau/alloc.zig").setAllocator;
/// Revert tooling allocation to the libc fallback.
pub const useDefaultAllocator = @import("luau/alloc.zig").useDefaultAllocator;

// ---- build configuration -----------------------------------------------------

/// Number of float lanes in the native vector type (3 or 4), matching the build.
pub const vector_size: u32 = build_options.vector_size;
/// Whether the native code generation (JIT) backend was compiled in.
pub const codegen_enabled: bool = build_options.codegen;
