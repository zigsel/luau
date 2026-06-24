//! Structured access to type-check diagnostics.
//!
//! `analysis.check` exposes error message strings only. This module type-checks
//! a single self-contained module and, per error, exposes a stable `Kind` enum,
//! the source `Location`, the rendered message, and — for variants with a cheap
//! typed string/name field that needs no live type printer — that field
//! (e.g. `UnknownSymbol.name`, `UnknownProperty.key`).
//!
//! Variants whose interesting payload is a `TypeId`/`TypePackId` (needing a live
//! type printer + arena) expose only kind + location + message here.

const std = @import("std");
const c = @import("bindings");

/// A 0-based source position.
pub const Position = struct {
    line: u32,
    column: u32,
};

/// A 0-based source span.
pub const Location = struct {
    begin: Position,
    end: Position,
};

/// Stable diagnostic kind. Values mirror the `TypeErrorData` variant order in
/// upstream `Luau/Error.h`. `unknown` is reported for an out-of-range index or a
/// shim-level exception.
pub const Kind = enum(c_int) {
    unknown = -1,
    type_mismatch = 0,
    unknown_symbol = 1,
    unknown_property = 2,
    not_a_table = 3,
    cannot_extend_table = 4,
    cannot_compare_unrelated_types = 5,
    only_tables_can_have_methods = 6,
    duplicate_type_definition = 7,
    count_mismatch = 8,
    function_does_not_take_self = 9,
    function_requires_self = 10,
    occurs_check_failed = 11,
    unknown_require = 12,
    incorrect_generic_parameter_count = 13,
    syntax_error = 14,
    code_too_complex = 15,
    unification_too_complex = 16,
    unknown_prop_but_found_like_prop = 17,
    generic_error = 18,
    internal_error = 19,
    constraint_solving_incomplete = 20,
    cannot_call_non_function = 21,
    extra_information = 22,
    deprecated_api_used = 23,
    module_has_cyclic_dependency = 24,
    illegal_require = 25,
    function_exits_without_returning = 26,
    duplicate_generic_parameter = 27,
    cannot_assign_to_never = 28,
    cannot_infer_binary_operation = 29,
    missing_properties = 30,
    swapped_generic_type_parameter = 31,
    optional_value_access = 32,
    missing_union_property = 33,
    types_are_unrelated = 34,
    normalization_too_complex = 35,
    type_pack_mismatch = 36,
    dynamic_property_lookup_on_extern_types_unsafe = 37,
    uninhabited_type_function = 38,
    uninhabited_type_pack_function = 39,
    where_clause_needed = 40,
    pack_where_clause_needed = 41,
    checked_function_call_error = 42,
    non_strict_function_definition_error = 43,
    property_access_violation = 44,
    checked_function_incorrect_args = 45,
    unexpected_type_in_subtyping = 46,
    unexpected_type_pack_in_subtyping = 47,
    explicit_function_annotation_recommended = 48,
    user_defined_type_function_error = 49,
    built_in_type_function_error = 50,
    reserved_identifier = 51,
    unexpected_array_like_table_item = 52,
    cannot_check_dynamic_string_format_calls = 53,
    generic_type_count_mismatch = 54,
    generic_type_pack_count_mismatch = 55,
    multiple_nonviable_overloads = 56,
    recursive_restraint_violation = 57,
    generic_bounds_mismatch = 58,
    unapplied_type_function = 59,
    instantiate_generics_on_non_function = 60,
    type_instantiation_count_mismatch = 61,
    ambiguous_function_call = 62,
    _,
};

/// One structured diagnostic. `message` and `field` borrow the owning
/// `Diagnostics` result's storage.
pub const Diagnostic = struct {
    kind: Kind,
    location: Location,
    /// Fully rendered message (always available).
    message: []const u8,
    /// A cheap typed string field (name/key/message) when the variant exposes
    /// one without a live type printer; otherwise `null`.
    field: ?[]const u8,
};

/// The collected diagnostics for a checked module. Owns its storage; call
/// `deinit`. Returned diagnostics borrow it.
pub const Diagnostics = struct {
    handle: *c.LuauDiagnostics,

    pub fn deinit(self: Diagnostics) void {
        c.luau_analysis_diagnostics_free(self.handle);
    }

    /// Whether the module type-checked with no errors.
    pub fn ok(self: Diagnostics) bool {
        return self.count() == 0;
    }
    pub fn count(self: Diagnostics) usize {
        return @intCast(c.luau_analysis_diagnostics_count(self.handle));
    }

    /// The `i`-th diagnostic (strings borrow this result's storage).
    pub fn get(self: Diagnostics, i: usize) Diagnostic {
        const begin = c.luau_analysis_diagnostics_position(self.handle, @intCast(i));
        const end = c.luau_analysis_diagnostics_end_position(self.handle, @intCast(i));
        const has_field = c.luau_analysis_diagnostics_has_field(self.handle, @intCast(i)) != 0;
        const field = c.luau_analysis_diagnostics_field(self.handle, @intCast(i));
        return .{
            .kind = @enumFromInt(c.luau_analysis_diagnostics_kind(self.handle, @intCast(i))),
            .location = .{
                .begin = .{ .line = begin.line, .column = begin.column },
                .end = .{ .line = end.line, .column = end.column },
            },
            .message = std.mem.span(c.luau_analysis_diagnostics_message(self.handle, @intCast(i))),
            .field = if (has_field) std.mem.span(field) else null,
        };
    }

    pub fn iterator(self: Diagnostics) Iterator {
        return .{ .diags = self, .n = self.count() };
    }

    pub const Iterator = struct {
        diags: Diagnostics,
        i: usize = 0,
        n: usize,
        pub fn next(it: *Iterator) ?Diagnostic {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.diags.get(it.i);
        }
    };
};

/// Type-check a single self-contained Luau `source` module and collect
/// structured diagnostics.
pub fn check(source: []const u8) Diagnostics {
    return .{ .handle = c.luau_analysis_diagnostics_check(source.ptr, source.len).? };
}
