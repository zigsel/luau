//! Idiomatic wrapper over Luau TYPE UTILITIES & MANIPULATION via the C++
//! Analysis shim.
//!
//! These operate over the `Type` / `TypePack` handles produced by a `Checker`
//! (see `types.zig`). Predicates are pure; reducers/simplifiers/clone and path
//! traversal mint new types into the owning checker's arena, so their results
//! stay valid until that checker is `deinit`'d. Utilities that need checker
//! context take the `Checker` as their first argument.

const std = @import("std");
const c = @import("bindings");
const types = @import("types.zig");

const Type = types.Type;
const TypePack = types.TypePack;
const Checker = types.Checker;

fn wrapType(p: ?*c.LuauType) ?Type {
    return .{ .handle = p orelse return null };
}
fn wrapPack(p: ?*c.LuauTypePack) ?TypePack {
    return .{ .handle = p orelse return null };
}

// ---- follow ----------------------------------------------------------------

/// Resolve bound/redirected types to their representative.
pub fn follow(t: Type) ?Type {
    return wrapType(c.luau_typeutils_follow(t.handle));
}

// ---- pointer-only predicates -----------------------------------------------

pub fn isNil(t: Type) bool {
    return c.luau_typeutils_is_nil(t.handle) != 0;
}
pub fn isBoolean(t: Type) bool {
    return c.luau_typeutils_is_boolean(t.handle) != 0;
}
pub fn isNumber(t: Type) bool {
    return c.luau_typeutils_is_number(t.handle) != 0;
}
pub fn isInteger(t: Type) bool {
    return c.luau_typeutils_is_integer(t.handle) != 0;
}
pub fn isString(t: Type) bool {
    return c.luau_typeutils_is_string(t.handle) != 0;
}
pub fn isThread(t: Type) bool {
    return c.luau_typeutils_is_thread(t.handle) != 0;
}
pub fn isBuffer(t: Type) bool {
    return c.luau_typeutils_is_buffer(t.handle) != 0;
}
/// Structurally `T?` (a union containing nil). Quick, not semantic.
pub fn isOptional(t: Type) bool {
    return c.luau_typeutils_is_optional(t.handle) != 0;
}
pub fn isTableUnion(t: Type) bool {
    return c.luau_typeutils_is_table_union(t.handle) != 0;
}
pub fn isTableIntersection(t: Type) bool {
    return c.luau_typeutils_is_table_intersection(t.handle) != 0;
}
pub fn isOverloadedFunction(t: Type) bool {
    return c.luau_typeutils_is_overloaded_function(t.handle) != 0;
}
pub fn maybeSingleton(t: Type) bool {
    return c.luau_typeutils_maybe_singleton(t.handle) != 0;
}
pub fn isGeneric(t: Type) bool {
    return c.luau_typeutils_is_generic(t.handle) != 0;
}
/// Whether `t` is the given primitive kind.
pub fn isPrim(t: Type, prim: types.PrimitiveKind) bool {
    return c.luau_typeutils_is_prim(t.handle, @intFromEnum(prim)) != 0;
}
/// Fast (incomplete) approximation of `sub <: super`.
pub fn fastIsSubtype(sub: Type, super: Type) bool {
    return c.luau_typeutils_fast_is_subtype(sub.handle, super.handle) != 0;
}
/// Approximately `false | nil` (syntactic, not semantic).
pub fn isApproximatelyFalsy(t: Type) bool {
    return c.luau_typeutils_is_approx_falsy(t.handle) != 0;
}
/// Approximately `~(false | nil)` (syntactic, not semantic).
pub fn isApproximatelyTruthy(t: Type) bool {
    return c.luau_typeutils_is_approx_truthy(t.handle) != 0;
}
/// A blocked / pending-expansion / unsolved-type-function type.
pub fn isBlocked(t: Type) bool {
    return c.luau_typeutils_is_blocked(t.handle) != 0;
}

// ---- predicates / manipulators needing checker context ---------------------

/// Whether `t` is a supertype of nil. Needs the checker's builtin types.
pub fn isOptionalType(checker: Checker, t: Type) bool {
    return c.luau_typeutils_is_optional_type(checker.handle, t.handle) != 0;
}

/// Remove nil from a union if another option remains. Allocates in the
/// checker's arena. Returns the (possibly new) type, or `null` on failure.
pub fn stripNil(checker: Checker, t: Type) ?Type {
    return wrapType(c.luau_typeutils_strip_nil(checker.handle, t.handle));
}

/// An approximate return pack for a function / union-of-functions, or `null`.
pub fn approximateReturn(t: Type) ?TypePack {
    return wrapPack(c.luau_typeutils_approximate_return(t.handle));
}

// ---- Simplify --------------------------------------------------------------

/// Simplify `a | b` into a single type, allocated in the checker's arena.
pub fn simplifyUnion(checker: Checker, a: Type, b: Type) ?Type {
    return wrapType(c.luau_typeutils_simplify_union(checker.handle, a.handle, b.handle));
}
/// Simplify `a & b` into a single type, allocated in the checker's arena.
pub fn simplifyIntersection(checker: Checker, a: Type, b: Type) ?Type {
    return wrapType(c.luau_typeutils_simplify_intersection(checker.handle, a.handle, b.handle));
}

/// The set relation between two types (mirrors `Luau::Relation`).
pub const Relation = enum(c_int) {
    disjoint = 0,
    coincident = 1,
    intersects = 2,
    subset = 3,
    superset = 4,
    unknown = -1,
    _,
};

/// The set relation between `a` and `b`.
pub fn relate(a: Type, b: Type) Relation {
    return @enumFromInt(c.luau_typeutils_relate(a.handle, b.handle));
}

// ---- Clone -----------------------------------------------------------------

/// Deep-clone `t` into the checker's arena. `shallow` clones only the top-level
/// constructor. Returns a new handle, or `null` on failure.
pub fn clone(checker: Checker, t: Type, shallow: bool) ?Type {
    return wrapType(c.luau_typeutils_clone(checker.handle, t.handle, @intFromBool(shallow)));
}

// ---- TypePath --------------------------------------------------------------

/// A relative path through a type/pack, built component-by-component and then
/// traversed from a root type. Independent of any checker; call `deinit`.
pub const Path = struct {
    handle: *c.LuauTypePath,

    pub fn init() ?Path {
        return .{ .handle = c.luau_typepath_new() orelse return null };
    }
    pub fn deinit(self: Path) void {
        c.luau_typepath_free(self.handle);
    }

    /// Look at the read type of property `name`. `name` need not be sentinel-terminated.
    pub fn readProp(self: Path, name: [:0]const u8) Path {
        c.luau_typepath_read_prop(self.handle, name.ptr);
        return self;
    }
    pub fn writeProp(self: Path, name: [:0]const u8) Path {
        c.luau_typepath_write_prop(self.handle, name.ptr);
        return self;
    }
    /// Index into a union/intersection list or pack element.
    pub fn index(self: Path, i: usize) Path {
        c.luau_typepath_index(self.handle, i);
        return self;
    }
    pub fn metatable(self: Path) Path {
        c.luau_typepath_metatable(self.handle);
        return self;
    }
    pub fn lowerBound(self: Path) Path {
        c.luau_typepath_lower_bound(self.handle);
        return self;
    }
    pub fn upperBound(self: Path) Path {
        c.luau_typepath_upper_bound(self.handle);
        return self;
    }
    pub fn indexKey(self: Path) Path {
        c.luau_typepath_index_key(self.handle);
        return self;
    }
    pub fn indexValue(self: Path) Path {
        c.luau_typepath_index_value(self.handle);
        return self;
    }
    pub fn negated(self: Path) Path {
        c.luau_typepath_negated(self.handle);
        return self;
    }
    pub fn variadic(self: Path) Path {
        c.luau_typepath_variadic(self.handle);
        return self;
    }
    pub fn args(self: Path) Path {
        c.luau_typepath_args(self.handle);
        return self;
    }
    pub fn rets(self: Path) Path {
        c.luau_typepath_rets(self.handle);
        return self;
    }

    /// Stringify the path. `human` produces the error-reporting form. Caller
    /// owns the returned slice.
    pub fn toString(self: Path, allocator: std.mem.Allocator, human: bool) ![]u8 {
        const out = c.luau_typepath_tostring(self.handle, @intFromBool(human)) orelse return error.OutOfMemory;
        defer std.c.free(out);
        return allocator.dupe(u8, std.mem.span(out));
    }

    /// Traverse from `root` to a type endpoint. Result is owned by `root`'s checker.
    pub fn traverseToType(self: Path, root: Type) ?Type {
        return wrapType(c.luau_typepath_traverse_to_type(root.handle, self.handle));
    }
    /// Traverse from `root` to a pack endpoint. Result is owned by `root`'s checker.
    pub fn traverseToPack(self: Path, root: Type) ?TypePack {
        return wrapPack(c.luau_typepath_traverse_to_pack(root.handle, self.handle));
    }
};
