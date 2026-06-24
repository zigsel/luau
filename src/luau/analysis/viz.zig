//! Idiomatic wrapper over VISUALIZATION / serialization of inferred Luau types
//! via the C++ Analysis shim.
//!
//! Operates on the `Type` / `TypePack` handles from `analysis.types` (each wraps
//! a Luau `TypeId` / `TypePackId`): render a type's object graph as a Graphviz
//! `dot` string, or stringify it with the full set of `ToStringOptions`.
//!
//! NOTE: a usable type-graph JSON serializer is not exposed — Luau's JsonEmitter
//! has no public `write` overload for `TypeId`/`TypePackId`. Use AST-level JSON
//! (`ast.json`) or `toString`/`toDot` here instead.

const std = @import("std");
const c = @import("bindings");
const types = @import("types.zig");

/// Options mirroring `Luau::ToStringOptions`. A value of 0 for the size limits
/// means "use Luau's compiled-in default".
pub const ToStringOptions = struct {
    exhaustive: bool = false,
    use_line_breaks: bool = false,
    function_type_arguments: bool = false,
    hide_table_kind: bool = false,
    hide_named_function_type_parameters: bool = false,
    hide_function_self_argument: bool = false,
    hide_table_alias_expansions: bool = false,
    use_question_marks: bool = true,
    ignore_synthetic_name: bool = false,
    max_table_length: usize = 0,
    max_type_length: usize = 0,
    composite_types_single_line_limit: usize = 0,

    fn toC(self: ToStringOptions) c.LuauToStringOptions {
        return .{
            .exhaustive = @intFromBool(self.exhaustive),
            .use_line_breaks = @intFromBool(self.use_line_breaks),
            .function_type_arguments = @intFromBool(self.function_type_arguments),
            .hide_table_kind = @intFromBool(self.hide_table_kind),
            .hide_named_function_type_parameters = @intFromBool(self.hide_named_function_type_parameters),
            .hide_function_self_argument = @intFromBool(self.hide_function_self_argument),
            .hide_table_alias_expansions = @intFromBool(self.hide_table_alias_expansions),
            .use_question_marks = @intFromBool(self.use_question_marks),
            .ignore_synthetic_name = @intFromBool(self.ignore_synthetic_name),
            .max_table_length = self.max_table_length,
            .max_type_length = self.max_type_length,
            .composite_types_single_line_limit = self.composite_types_single_line_limit,
        };
    }
};

/// Graphviz `dot` rendering options (mirrors `Luau::ToDotOptions`).
pub const DotOptions = struct {
    show_pointers: bool = true,
    duplicate_primitives: bool = true,
};

/// The status flags reported by `toStringDetailed`.
pub const DetailedFlags = struct {
    invalid: bool,
    @"error": bool,
    cycle: bool,
    truncated: bool,
};

pub const DetailedResult = struct {
    string: []u8,
    flags: DetailedFlags,
};

fn dupOrNull(allocator: std.mem.Allocator, ptr: [*c]u8) ![]u8 {
    if (ptr == null) return error.VizFailed;
    defer std.c.free(ptr);
    return allocator.dupe(u8, std.mem.span(ptr));
}

/// Render `t`'s type object graph as a Graphviz `dot` string. Caller owns it.
pub fn typeToDot(allocator: std.mem.Allocator, t: types.Type, opts: DotOptions) ![]u8 {
    return dupOrNull(allocator, c.luau_viz_type_to_dot(
        t.handle,
        @intFromBool(opts.show_pointers),
        @intFromBool(opts.duplicate_primitives),
    ));
}

/// Render a type pack's object graph as a Graphviz `dot` string.
pub fn typePackToDot(allocator: std.mem.Allocator, tp: types.TypePack, opts: DotOptions) ![]u8 {
    return dupOrNull(allocator, c.luau_viz_typepack_to_dot(
        tp.handle,
        @intFromBool(opts.show_pointers),
        @intFromBool(opts.duplicate_primitives),
    ));
}

/// `Luau::toString(TypeId, opts)`. Caller owns the returned slice.
pub fn typeToString(allocator: std.mem.Allocator, t: types.Type, opts: ToStringOptions) ![]u8 {
    var co = opts.toC();
    return dupOrNull(allocator, c.luau_viz_type_to_string(t.handle, &co));
}

/// `Luau::toString(TypePackId, opts)`. Caller owns the returned slice.
pub fn typePackToString(allocator: std.mem.Allocator, tp: types.TypePack, opts: ToStringOptions) ![]u8 {
    var co = opts.toC();
    return dupOrNull(allocator, c.luau_viz_typepack_to_string(tp.handle, &co));
}

/// `Luau::toStringDetailed(TypeId, opts)`: the string plus status flags. Caller
/// owns `result.string`.
pub fn typeToStringDetailed(allocator: std.mem.Allocator, t: types.Type, opts: ToStringOptions) !DetailedResult {
    var co = opts.toC();
    var out: [*c]u8 = null;
    var invalid: c_int = 0;
    var err: c_int = 0;
    var cycle: c_int = 0;
    var truncated: c_int = 0;
    const ok = c.luau_viz_type_to_string_detailed(t.handle, &co, &out, &invalid, &err, &cycle, &truncated);
    if (ok == 0 or out == null) return error.VizFailed;
    defer std.c.free(out);
    return .{
        .string = try allocator.dupe(u8, std.mem.span(out)),
        .flags = .{
            .invalid = invalid != 0,
            .@"error" = err != 0,
            .cycle = cycle != 0,
            .truncated = truncated != 0,
        },
    };
}
