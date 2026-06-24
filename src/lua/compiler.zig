//! The Luau bytecode compiler (`luacode.h`).

const std = @import("std");
const c = @import("bindings");
const Lua = @import("lua.zig").Lua;

/// Options controlling `compile`. Mirrors `struct lua_CompileOptions`.
pub const CompileOptions = struct {
    /// 0 none, 1 baseline (debuggable), 2 includes inlining.
    optimization_level: i32 = 1,
    /// 0 none, 1 line info & names, 2 full (locals/upvalues).
    debug_level: i32 = 1,
    /// 0 native modules only, 1 all modules.
    type_info_level: i32 = 0,
    /// 0 none, 1 statement, 2 statement+expression.
    coverage_level: i32 = 0,

    /// Alternative builtin used to construct vectors (library + constructor).
    vector_lib: ?[:0]const u8 = null,
    vector_ctor: ?[:0]const u8 = null,
    /// Alternative vector type name for type tables.
    vector_type: ?[:0]const u8 = null,

    /// Null-terminated array of globals treated as mutable (disables import opt).
    mutable_globals: ?[*:null]const ?[*:0]const u8 = null,
    /// Null-terminated array of userdata type names included in type info.
    userdata_types: ?[*:null]const ?[*:0]const u8 = null,
    /// Null-terminated array of library globals with known members.
    libraries_with_known_members: ?[*:null]const ?[*:0]const u8 = null,
    library_member_type_cb: c.lua_LibraryMemberTypeCallback = null,
    library_member_constant_cb: c.lua_LibraryMemberConstantCallback = null,
    /// Null-terminated array of "lib.name" builtins to not fast-call.
    disabled_builtins: ?[*:null]const ?[*:0]const u8 = null,

    fn toC(self: CompileOptions) c.lua_CompileOptions {
        return .{
            .optimizationLevel = self.optimization_level,
            .debugLevel = self.debug_level,
            .typeInfoLevel = self.type_info_level,
            .coverageLevel = self.coverage_level,
            .vectorLib = if (self.vector_lib) |s| s.ptr else null,
            .vectorCtor = if (self.vector_ctor) |s| s.ptr else null,
            .vectorType = if (self.vector_type) |s| s.ptr else null,
            .mutableGlobals = @ptrCast(self.mutable_globals),
            .userdataTypes = @ptrCast(self.userdata_types),
            .librariesWithKnownMembers = @ptrCast(self.libraries_with_known_members),
            .libraryMemberTypeCb = self.library_member_type_cb,
            .libraryMemberConstantCb = self.library_member_constant_cb,
            .disabledBuiltins = @ptrCast(self.disabled_builtins),
        };
    }
};

/// Compile `source` to bytecode, returning a slice owned by `allocator`.
///
/// On a compilation error Luau returns *error bytecode* (which raises when
/// loaded) rather than failing here; `Lua.loadBytecode` surfaces it as
/// `error.Syntax`. Use `loadString`/`doString` for the common path.
pub fn compile(allocator: std.mem.Allocator, source: []const u8, options: CompileOptions) ![]u8 {
    var opts = options.toC();
    var len: usize = 0;
    const bc = c.luau_compile(source.ptr, source.len, &opts, &len);
    defer std.c.free(@ptrCast(bc));
    if (bc == null) return error.OutOfMemory;
    return allocator.dupe(u8, bc[0..len]);
}

/// Whether `bytecode` encodes a compilation error rather than a function.
pub fn isErrorBytecode(bytecode: []const u8) bool {
    return bytecode.len > 0 and bytecode[0] == 0;
}

// ---- compile-time constant setters (for libraryMemberConstantCb) ------------

pub const CompileConstant = c.lua_CompileConstant;

pub fn setConstantNil(constant: *CompileConstant) void {
    c.luau_set_compile_constant_nil(constant);
}
pub fn setConstantBoolean(constant: *CompileConstant, b: bool) void {
    c.luau_set_compile_constant_boolean(constant, @intFromBool(b));
}
pub fn setConstantNumber(constant: *CompileConstant, n: f64) void {
    c.luau_set_compile_constant_number(constant, n);
}
pub fn setConstantInteger64(constant: *CompileConstant, n: i64) void {
    c.luau_set_compile_constant_integer64(constant, n);
}
pub fn setConstantVector(constant: *CompileConstant, x: f32, y: f32, z: f32, w: f32) void {
    c.luau_set_compile_constant_vector(constant, x, y, z, w);
}
pub fn setConstantString(constant: *CompileConstant, s: []const u8) void {
    c.luau_set_compile_constant_string(constant, s.ptr, s.len);
}
