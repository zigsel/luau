//! Idiomatic wrapper over TYPE NORMALIZATION and STRUCTURAL EQUALITY of
//! inferred Luau types via the C++ Analysis shim.
//!
//! These operate over the `Type` handles produced by a `Checker` (see
//! `types.zig`). Each `Type` carries its owning checker, from which the shim
//! sources the analysis context (arena + builtin types + solver mode) needed to
//! build a `Luau::Normalizer`. Normalized results are minted into the owning
//! checker's arena, so they stay valid until that checker is `deinit`'d.
//!
//! NOT exposed (and why): `Luau::Substitution` is an abstract solver pass that
//! must be subclassed inside the type solver and needs a live transaction log /
//! free types, so it is not reconstructible from a finished checker. The
//! solver-internal `Normalizer` mutators (union/intersect/negate over
//! `NormalizedType` aggregates, inhabitance checks) are likewise skipped. See
//! shim/analysis/normalize.h for the full rationale.

const std = @import("std");
const c = @import("bindings");
const types = @import("types.zig");

const Type = types.Type;

fn wrapType(p: ?*c.LuauType) ?Type {
    return .{ .handle = p orelse return null };
}

/// Normalize `t` into the solver's canonical disjunctive-normal form and return
/// the resulting type (minted into the owning checker's arena). Returns `null`
/// if the type is too complex to normalize or an internal error occurs.
pub fn normalize(t: Type) ?Type {
    return wrapType(c.luau_normalize_type(t.handle));
}

/// The stringified normal form of `t`. The shim returns a `malloc`'d C string;
/// we copy it into `allocator` (freeing the original) and hand back an
/// allocator-owned slice the caller frees with `allocator.free`. Returns `null`
/// on failure.
pub fn toStringAlloc(allocator: std.mem.Allocator, t: Type) !?[]u8 {
    const raw = c.luau_normalize_tostring(t.handle) orelse return null;
    defer std.c.free(raw);
    const span = std.mem.span(raw);
    return try allocator.dupe(u8, span);
}

// DROPPED: unionOf / intersectionOf — the underlying Normalizer::unionType /
// intersectionType are private members of Luau::Normalizer and cannot be bound.

/// STRUCTURAL (syntactic, cycle-safe) type equality. This is NOT semantic
/// equivalence: it compares the literal shapes, so two semantically equal but
/// differently-spelled types may compare unequal. Returns `null` on error.
pub fn structurallyEqual(a: Type, b: Type) ?bool {
    return switch (c.luau_type_structurally_equal(a.handle, b.handle)) {
        1 => true,
        0 => false,
        else => null,
    };
}
