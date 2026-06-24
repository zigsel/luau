//! Idiomatic wrapper over MODULE / SCOPE / DEFINITION inspection of a checked
//! Luau module via the C++ Analysis shim.
//!
//! `Module.check` type-checks a self-contained module (retaining the full type
//! graph) and exposes module-level metadata, the module's return type pack, the
//! top-level scope's bindings (name + inferred type + declaration location), and
//! the module's exported type aliases. Every `Type`/`TypePack` it returns is one
//! of the `types` shim's handles, so the full type-inspection API applies; they
//! borrow this `Module` and must not be used after `deinit`.

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;
const types = @import("types.zig");

/// A source span (0-based) for a declaration.
pub const Location = struct {
    begin: Position,
    end: Position,
};

/// A type error reported during checking.
pub const TypeError = struct {
    message: []const u8,
    position: Position,
};

/// A top-level scope binding: its source name, inferred type, declaration
/// location and deprecation flag.
pub const Binding = struct {
    name: []const u8,
    type: ?types.Type,
    location: Location,
    deprecated: bool,
};

/// An exported type alias: its name and underlying type. The type is only
/// directly meaningful when the alias has no type parameters.
pub const ExportedType = struct {
    name: []const u8,
    type: ?types.Type,
};

pub const Module = struct {
    handle: *c.LuauModule,

    /// Type-check the self-contained Luau module `src` and return a `Module`
    /// that owns the result. Call `deinit`.
    pub fn check(src: []const u8) Module {
        return .{ .handle = c.luau_module_check(src.ptr, src.len).? };
    }

    pub fn deinit(self: Module) void {
        c.luau_module_free(self.handle);
    }

    // ---- module-level info ----

    /// The module's name. Caller owns the returned slice.
    pub fn name(self: Module, allocator: std.mem.Allocator) !?[]u8 {
        const out = c.luau_module_name(self.handle) orelse return null;
        defer std.c.free(out);
        return try allocator.dupe(u8, std.mem.span(out));
    }

    /// The module's human-readable name. Caller owns the returned slice.
    pub fn humanName(self: Module, allocator: std.mem.Allocator) !?[]u8 {
        const out = c.luau_module_human_name(self.handle) orelse return null;
        defer std.c.free(out);
        return try allocator.dupe(u8, std.mem.span(out));
    }

    /// Whether the module produced a checked `Module` object.
    pub fn checked(self: Module) bool {
        return c.luau_module_checked(self.handle) != 0;
    }

    pub fn errorCount(self: Module) usize {
        return @intCast(c.luau_module_error_count(self.handle));
    }

    pub fn getError(self: Module, i: usize) TypeError {
        const msg = c.luau_module_error_message(self.handle, @intCast(i));
        const pos = c.luau_module_error_position(self.handle, @intCast(i));
        return .{
            .message = std.mem.span(msg),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }

    pub fn errors(self: Module) ErrorIterator {
        return .{ .module = self, .n = self.errorCount() };
    }

    pub const ErrorIterator = struct {
        module: Module,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ErrorIterator) ?TypeError {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.module.getError(it.i);
        }
    };

    pub fn timedOut(self: Module) bool {
        return c.luau_module_timed_out(self.handle) != 0;
    }

    pub fn cancelled(self: Module) bool {
        return c.luau_module_cancelled(self.handle) != 0;
    }

    // ---- module return type ----

    /// The module's return type pack, or `null` if none.
    pub fn returnType(self: Module) ?types.TypePack {
        return types.TypePack.wrap(c.luau_module_return_type(self.handle));
    }

    // ---- top-level scope bindings ----

    pub fn bindingCount(self: Module) usize {
        return @intCast(c.luau_module_binding_count(self.handle));
    }

    pub fn binding(self: Module, i: usize) ?Binding {
        const idx: c_int = @intCast(i);
        const nm = c.luau_module_binding_name(self.handle, idx) orelse return null;
        const loc = c.luau_module_binding_location(self.handle, idx);
        return .{
            .name = std.mem.span(nm),
            .type = types.Type.wrap(c.luau_module_binding_type(self.handle, idx)),
            .location = .{
                .begin = .{ .line = loc.begin_line, .column = loc.begin_column },
                .end = .{ .line = loc.end_line, .column = loc.end_column },
            },
            .deprecated = c.luau_module_binding_deprecated(self.handle, idx) != 0,
        };
    }

    pub fn bindings(self: Module) BindingIterator {
        return .{ .module = self, .n = self.bindingCount() };
    }

    pub const BindingIterator = struct {
        module: Module,
        i: usize = 0,
        n: usize,
        pub fn next(it: *BindingIterator) ?Binding {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.module.binding(it.i);
        }
    };

    /// Inferred type of the top-level binding named `name`, or `null`.
    pub fn lookup(self: Module, binding_name: [:0]const u8) ?types.Type {
        return types.Type.wrap(c.luau_module_binding_lookup(self.handle, binding_name.ptr));
    }

    // ---- exported types ----

    pub fn exportedTypeCount(self: Module) usize {
        return @intCast(c.luau_module_exported_type_count(self.handle));
    }

    pub fn exportedType(self: Module, i: usize) ?ExportedType {
        const idx: c_int = @intCast(i);
        const nm = c.luau_module_exported_type_name(self.handle, idx) orelse return null;
        return .{
            .name = std.mem.span(nm),
            .type = types.Type.wrap(c.luau_module_exported_type(self.handle, idx)),
        };
    }

    pub fn exportedTypes(self: Module) ExportedTypeIterator {
        return .{ .module = self, .n = self.exportedTypeCount() };
    }

    pub const ExportedTypeIterator = struct {
        module: Module,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ExportedTypeIterator) ?ExportedType {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.module.exportedType(it.i);
        }
    };
};
