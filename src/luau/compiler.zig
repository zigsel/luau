//! The Luau compiler — one namespace: source/AST → bytecode, options, compile
//! constants, and structured (location-aware) compile errors.

const std = @import("std");
const c = @import("bindings");
const Position = @import("ast.zig").Position;
const cc = @import("../lua/compiler.zig");

pub const CompileOptions = cc.CompileOptions;
/// Compile Luau source to bytecode, returning a slice owned by the allocator.
pub const compile = cc.compile;
/// Whether a bytecode blob encodes a compile error rather than a function.
pub const isErrorBytecode = cc.isErrorBytecode;

pub const CompileConstant = cc.CompileConstant;
pub const setConstantNil = cc.setConstantNil;
pub const setConstantBoolean = cc.setConstantBoolean;
pub const setConstantNumber = cc.setConstantNumber;
pub const setConstantInteger64 = cc.setConstantInteger64;
pub const setConstantVector = cc.setConstantVector;
pub const setConstantString = cc.setConstantString;

/// Structured compile errors (location + message), via `Luau::compileOrThrow`.
///
/// The plain `compile` path always yields a bytecode blob — on failure the error
/// is encoded *into* the bytecode for the VM loader. `errors.check` instead
/// reports the structured error (source position + message) directly.
pub const errors = struct {
    /// A single compile (or parse) error: a message and the source position
    /// where it occurred. `message` borrows the owning `Check`'s storage.
    pub const CompileError = struct {
        message: []const u8,
        position: Position,
    };

    /// The outcome of a structured compile. Owns the handle; call `deinit`.
    pub const Check = struct {
        handle: *c.LuauCompileErrors,

        pub fn deinit(self: Check) void {
            c.luau_compile_errors_free(self.handle);
        }
        /// Whether compilation succeeded with no errors.
        pub fn ok(self: Check) bool {
            return c.luau_compile_errors_ok(self.handle) != 0;
        }
        /// Number of captured errors.
        pub fn count(self: Check) usize {
            return @intCast(c.luau_compile_errors_count(self.handle));
        }
        /// The i-th error, or null if out of range.
        pub fn at(self: Check, i: usize) ?CompileError {
            const msg = c.luau_compile_errors_message(self.handle, @intCast(i)) orelse return null;
            const pos = c.luau_compile_errors_position(self.handle, @intCast(i));
            return .{
                .message = std.mem.span(msg),
                .position = .{ .line = pos.line, .column = pos.column },
            };
        }
    };

    /// Compile `src` and capture any structured errors. `optimization_level` and
    /// `debug_level` mirror the compiler options (defaults 1/1). `deinit` the result.
    pub fn check(src: []const u8, optimization_level: i32, debug_level: i32) error{OutOfMemory}!Check {
        const handle = c.luau_compile_errors_check(src.ptr, src.len, optimization_level, debug_level) orelse
            return error.OutOfMemory;
        return .{ .handle = handle };
    }
};
