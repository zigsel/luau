//! Idiomatic wrapper over multi-module Luau type checking (via the C++ shim).
//!
//! A `Project` holds a set of named in-memory modules where one may `require`
//! another (by constant-string module name). `check(entry)` type-checks the
//! entry module and every module it transitively requires.

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

/// A type error reported by the checker, tagged with the module it came from.
pub const ProjectError = struct {
    /// The module the error was reported in.
    module: []const u8,
    message: []const u8,
    position: Position,
};

/// A multi-module type-checking project. Owns its storage; call `deinit`.
pub const Project = struct {
    handle: *c.LuauProject,

    pub fn init() Project {
        return .{ .handle = c.luau_project_new().? };
    }

    pub fn deinit(self: Project) void {
        c.luau_project_free(self.handle);
    }

    /// Register (or replace) a module's source under `name`.
    pub fn addModule(self: Project, name: []const u8, src: []const u8) void {
        c.luau_project_add_module(self.handle, name.ptr, name.len, src.ptr, src.len);
    }

    /// Type-check `entry` and every module it (transitively) requires.
    pub fn check(self: Project, entry: [:0]const u8) void {
        c.luau_project_check(self.handle, entry.ptr);
    }

    /// Number of errors collected by the last `check`, across all modules.
    pub fn errorCount(self: Project) usize {
        return @intCast(c.luau_project_error_count(self.handle));
    }

    /// The `i`-th error (strings borrow the project's storage).
    pub fn getError(self: Project, i: usize) ProjectError {
        return .{
            .module = std.mem.span(c.luau_project_error_module_name(self.handle, @intCast(i))),
            .message = std.mem.span(c.luau_project_error_message(self.handle, @intCast(i))),
            .position = blk: {
                const p = c.luau_project_error_position(self.handle, @intCast(i));
                break :blk .{ .line = p.line, .column = p.column };
            },
        };
    }

    /// Whether the project type-checked with no errors.
    pub fn ok(self: Project) bool {
        return self.errorCount() == 0;
    }

    pub fn errors(self: Project) ErrorIterator {
        return .{ .project = self, .n = self.errorCount() };
    }

    pub const ErrorIterator = struct {
        project: Project,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ErrorIterator) ?ProjectError {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.project.getError(it.i);
        }
    };
};
