//! Test aggregator — `zig build test` runs every test in these files.

test {
    _ = @import("embedding.zig");
    _ = @import("coroutines.zig");
    _ = @import("require.zig");
    _ = @import("alloc.zig");
    _ = @import("require_fs.zig");
    _ = @import("closures.zig");
    _ = @import("ergonomics.zig");
    _ = @import("declare.zig");
    _ = @import("marshalling.zig");
    _ = @import("usertype.zig");
    _ = @import("compile.zig");
    _ = @import("tooling.zig");
    _ = @import("tooling_shim.zig");
    _ = @import("astbuilder.zig");
    _ = @import("ast_types.zig");
    _ = @import("analysis_typegraph.zig");
    _ = @import("config_aliases.zig");
    _ = @import("diagnostics.zig");
    _ = @import("requirepath.zig");
    _ = @import("signatures.zig");
    _ = @import("ast.zig");
    _ = @import("bytecode.zig");
    _ = @import("codegen.zig");
    _ = @import("analysis_typeops.zig");
    _ = @import("analysis_tools.zig");
}
