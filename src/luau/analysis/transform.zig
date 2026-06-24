//! Idiomatic wrapper over Luau deep type TRANSFORMS via the C++ Analysis shim.
//!
//! These drive the solver-internal type transformations (instantiation,
//! anyification, type-function application, generalization) over the `Type`
//! handles produced by a `Checker` (see `types.zig`). Each transform mints its
//! result into the owning checker's arena, so the returned `Type` stays valid
//! until that checker is `deinit`'d. Every transform takes the `Checker` as its
//! first argument since it needs the checker's type-arena / builtin-types /
//! module scope context.
//!
//! Only transforms that are SAFE to run against a finished checker are exposed.
//! `quantify`, `generalizeType`/`generalizeTypePack`, and `instantiate2` are
//! deliberately not bound because they require live solver state (in-place
//! mutation of shared types, tracked polarity/use-counts, or a live `Subtyping`)
//! and would assert/crash or corrupt the type graph. See `shim/analysis/transforms.h`.

const std = @import("std");
const c = @import("bindings");
const types = @import("types.zig");

const Type = types.Type;
const Checker = types.Checker;

fn wrapType(p: ?*c.LuauType) ?Type {
    return .{ .handle = p orelse return null };
}

/// Instantiate a generic function type: return a copy with its outermost
/// generics replaced by fresh free types. Non-generic inputs come back
/// essentially unchanged. Returns `null` on failure (recursion limit / error).
pub fn instantiate(checker: Checker, t: Type) ?Type {
    return wrapType(c.luau_transforms_instantiate(checker.handle, t.handle));
}

/// Replace every free type/pack reachable from `t` by `any`. On a finished
/// module (usually free of free types) this is effectively a structural copy.
/// Returns `null` on failure / normalization-too-complex.
pub fn anyify(checker: Checker, t: Type) ?Type {
    return wrapType(c.luau_transforms_anyify(checker.handle, t.handle));
}

/// Apply a generic function type to `args`: substitute each of `fn`'s ordinary
/// type generics by the corresponding argument and return the resulting type.
/// Returns `null` if `fn` is not a generic function, the argument count does not
/// match the generic count, or on internal error.
pub fn applyTypeFunction(checker: Checker, func: Type, args: []const Type) ?Type {
    // `Type` is a single-field struct wrapping `*c.LuauType`, so a slice of
    // `Type` is layout-compatible with an array of `LuauType*`.
    const base: [*c]const ?*c.LuauType = @ptrCast(args.ptr);
    return wrapType(c.luau_transforms_apply_type_function(
        checker.handle,
        func.handle,
        base,
        @intCast(args.len),
    ));
}

/// Generalize `t`, replacing free types by their bounds (making a partially
/// inferred function polymorphic). On a graph with no remaining free types this
/// typically returns `t` unchanged. Returns `null` on failure (resource limits).
pub fn generalize(checker: Checker, t: Type) ?Type {
    return wrapType(c.luau_transforms_generalize(checker.handle, t.handle));
}
