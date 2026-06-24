//! Idiomatic wrapper over STRUCTURAL INSPECTION of inferred Luau types — the
//! type object graph — via the C++ Analysis shim.
//!
//! A `Checker` owns the underlying Frontend + checked module, so every `Type`
//! and `TypePack` derived from it borrows the checker's storage and stays valid
//! until the checker is `deinit`'d.

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

/// The stable structural kind of an inferred type (mirrors `LuauTypeKind`).
pub const Kind = enum(c_int) {
    primitive = 0,
    singleton = 1,
    function = 2,
    table = 3,
    metatable = 4,
    class = 5,
    @"union" = 6,
    intersection = 7,
    generic = 8,
    free = 9,
    bound = 10,
    any = 11,
    unknown = 12,
    never = 13,
    @"error" = 14,
    negation = 15,
    unknown_kind = 16,
    _,
};

/// The concrete kind of a `.primitive` type (mirrors `LuauPrimitiveKind`).
pub const PrimitiveKind = enum(c_int) {
    nil = 0,
    boolean = 1,
    number = 2,
    integer = 3,
    string = 4,
    thread = 5,
    function = 6,
    table = 7,
    buffer = 8,
    unknown = 9,
    _,
};

/// Which literal a `.singleton` type carries.
pub const SingletonKind = enum(c_int) {
    none = 0,
    boolean = 1,
    string = 2,
    _,
};

/// A type error reported by the checker.
pub const TypeError = struct {
    message: []const u8,
    position: Position,
};

/// A handle to a single inferred type in the checker's type graph. Borrows the
/// owning `Checker`; do not use after the checker is freed.
pub const Type = struct {
    handle: *c.LuauType,

    pub fn kind(self: Type) Kind {
        return @enumFromInt(c.luau_type_kind(self.handle));
    }

    /// `Luau::toString` of this type. Caller owns the returned slice.
    pub fn toString(self: Type, allocator: std.mem.Allocator) ![]u8 {
        const out = c.luau_type_tostring(self.handle) orelse return error.OutOfMemory;
        defer std.c.free(out);
        return allocator.dupe(u8, std.mem.span(out));
    }

    // ---- primitive ----
    pub fn primitiveKind(self: Type) PrimitiveKind {
        return @enumFromInt(c.luau_type_primitive_kind(self.handle));
    }
    pub fn primitiveName(self: Type) ?[]const u8 {
        return spanOpt(c.luau_type_primitive_name(self.handle));
    }

    // ---- singleton ----
    pub fn singletonKind(self: Type) SingletonKind {
        return @enumFromInt(c.luau_type_singleton_kind(self.handle));
    }
    pub fn singletonBool(self: Type) ?bool {
        if (self.singletonKind() != .boolean) return null;
        return c.luau_type_singleton_bool(self.handle) != 0;
    }
    pub fn singletonString(self: Type) ?[]const u8 {
        return spanOpt(c.luau_type_singleton_string(self.handle));
    }

    // ---- function ----
    pub fn functionArgs(self: Type) ?TypePack {
        return TypePack.wrap(c.luau_type_function_args(self.handle));
    }
    pub fn functionRets(self: Type) ?TypePack {
        return TypePack.wrap(c.luau_type_function_rets(self.handle));
    }

    // ---- table ----
    pub fn tablePropCount(self: Type) usize {
        return @intCast(c.luau_type_table_prop_count(self.handle));
    }
    pub fn tablePropName(self: Type, i: usize) ?[]const u8 {
        return spanOpt(c.luau_type_table_prop_name(self.handle, @intCast(i)));
    }
    pub fn tablePropType(self: Type, i: usize) ?Type {
        return Type.wrap(c.luau_type_table_prop_type(self.handle, @intCast(i)));
    }
    pub fn tableHasIndexer(self: Type) bool {
        return c.luau_type_table_has_indexer(self.handle) != 0;
    }
    pub fn tableProps(self: Type) PropIterator {
        return .{ .ty = self, .n = self.tablePropCount() };
    }

    pub const Prop = struct { name: []const u8, type: Type };
    pub const PropIterator = struct {
        ty: Type,
        i: usize = 0,
        n: usize,
        pub fn next(it: *PropIterator) ?Prop {
            while (it.i < it.n) {
                const i = it.i;
                it.i += 1;
                const name = it.ty.tablePropName(i) orelse continue;
                const t = it.ty.tablePropType(i) orelse continue;
                return .{ .name = name, .type = t };
            }
            return null;
        }
    };

    // ---- metatable ----
    pub fn metatableTable(self: Type) ?Type {
        return Type.wrap(c.luau_type_metatable_table(self.handle));
    }
    pub fn metatableMetatable(self: Type) ?Type {
        return Type.wrap(c.luau_type_metatable_metatable(self.handle));
    }

    // ---- union / intersection ----
    pub fn unionCount(self: Type) usize {
        return @intCast(c.luau_type_union_count(self.handle));
    }
    pub fn unionAt(self: Type, i: usize) ?Type {
        return Type.wrap(c.luau_type_union_at(self.handle, @intCast(i)));
    }
    pub fn unionOptions(self: Type) ChildIterator {
        return .{ .ty = self, .n = self.unionCount(), .at = unionAt };
    }
    pub fn intersectionCount(self: Type) usize {
        return @intCast(c.luau_type_intersection_count(self.handle));
    }
    pub fn intersectionAt(self: Type, i: usize) ?Type {
        return Type.wrap(c.luau_type_intersection_at(self.handle, @intCast(i)));
    }
    pub fn intersectionParts(self: Type) ChildIterator {
        return .{ .ty = self, .n = self.intersectionCount(), .at = intersectionAt };
    }

    pub const ChildIterator = struct {
        ty: Type,
        i: usize = 0,
        n: usize,
        at: *const fn (Type, usize) ?Type,
        pub fn next(it: *ChildIterator) ?Type {
            while (it.i < it.n) {
                const i = it.i;
                it.i += 1;
                if (it.at(it.ty, i)) |t| return t;
            }
            return null;
        }
    };

    // ---- class (extern type) ----
    pub fn className(self: Type) ?[]const u8 {
        return spanOpt(c.luau_type_class_name(self.handle));
    }
    pub fn classParent(self: Type) ?Type {
        return Type.wrap(c.luau_type_class_parent(self.handle));
    }

    // ---- negation ----
    pub fn negationInner(self: Type) ?Type {
        return Type.wrap(c.luau_type_negation_inner(self.handle));
    }

    // ---- generic ----
    pub fn genericName(self: Type) ?[]const u8 {
        return spanOpt(c.luau_type_generic_name(self.handle));
    }

    pub fn wrap(p: ?*c.LuauType) ?Type {
        return .{ .handle = p orelse return null };
    }
};

/// A handle to a type pack (function arguments / returns). Borrows the checker.
pub const TypePack = struct {
    handle: *c.LuauTypePack,

    pub fn len(self: TypePack) usize {
        return @intCast(c.luau_typepack_count(self.handle));
    }
    pub fn at(self: TypePack, i: usize) ?Type {
        return Type.wrap(c.luau_typepack_at(self.handle, @intCast(i)));
    }
    /// The variadic/generic tail of the pack, or `null` if fixed-length.
    pub fn tail(self: TypePack) ?TypePack {
        return TypePack.wrap(c.luau_typepack_tail(self.handle));
    }
    pub fn iterator(self: TypePack) Iterator {
        return .{ .tp = self, .n = self.len() };
    }
    pub const Iterator = struct {
        tp: TypePack,
        i: usize = 0,
        n: usize,
        pub fn next(it: *Iterator) ?Type {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.tp.at(it.i);
        }
    };

    pub fn wrap(p: ?*c.LuauTypePack) ?TypePack {
        return .{ .handle = p orelse return null };
    }
};

/// Owns a type-checked module and its inferred type graph. Call `deinit`.
pub const Checker = struct {
    handle: *c.LuauTypes,

    pub fn deinit(self: Checker) void {
        c.luau_types_free(self.handle);
    }

    pub fn errorCount(self: Checker) usize {
        return @intCast(c.luau_types_error_count(self.handle));
    }
    pub fn ok(self: Checker) bool {
        return self.errorCount() == 0;
    }
    pub fn getError(self: Checker, i: usize) TypeError {
        const msg = c.luau_types_error_message(self.handle, @intCast(i));
        const pos = c.luau_types_error_position(self.handle, @intCast(i));
        return .{
            .message = std.mem.span(msg),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }
    pub fn errors(self: Checker) ErrorIterator {
        return .{ .checker = self, .n = self.errorCount() };
    }
    pub const ErrorIterator = struct {
        checker: Checker,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ErrorIterator) ?TypeError {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.checker.getError(it.i);
        }
    };

    /// The inferred type of a top-level binding/global named `name`, or `null`.
    pub fn global(self: Checker, name: [:0]const u8) ?Type {
        return Type.wrap(c.luau_types_require_global(self.handle, name.ptr));
    }
};

/// Type-check the self-contained Luau module `src` (with the full type graph
/// retained) and return a `Checker` that owns the result. Call `deinit`.
pub fn check(src: []const u8) Checker {
    return .{ .handle = c.luau_types_check(src.ptr, src.len).? };
}

fn spanOpt(p: [*c]const u8) ?[]const u8 {
    if (p == null) return null;
    return std.mem.span(p);
}
