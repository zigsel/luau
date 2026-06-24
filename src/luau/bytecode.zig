//! Hand-level Luau bytecode (advanced; most users want `luau.compile`).

/// Emit bytecode with `BytecodeBuilder`: `emit.Builder`, the `emit.Op` opcodes,
/// and the static import/string/version helpers.
pub const emit = @import("bytecode/emit.zig");

/// The bytecode graph (`BytecodeGraph.h`): lift a function's bytecode into
/// blocks/instructions/constants for inspection and round-trip serialization.
pub const graph = @import("bytecode/graph.zig");
