//! Project Zig types into Luau type definitions — so the type checker and
//! autocomplete (`analysis.defs` / `luau-lsp`) understand your host API.
//!
//! `luauType(T)` renders a Zig type as a Luau type expression; `declsFor(T, name)`
//! wraps it in a `declare <name>: <type>` definition. Both are comptime and
//! allocation-free — the result is a comptime `[]const u8` you can pass straight
//! to `analysis.defs.checkWithDefinitions` or write to a `.d.luau` file.

const std = @import("std");

/// Render Zig type `T` as a Luau type expression (e.g. `f64` -> `"number"`,
/// `[]const u8` -> `"string"`, a struct -> `"{ … }"`, a fn -> `"(…) -> …"`).
pub fn luauType(comptime T: type) []const u8 {
    return comptime switch (@typeInfo(T)) {
        .bool => "boolean",
        .int, .comptime_int, .float, .comptime_float => "number",
        .void => "()",
        .@"enum" => "number",
        .optional => |o| luauType(o.child) ++ "?",
        .pointer => |p| if (p.size == .slice and p.child == u8)
            "string"
        else if (p.size == .slice or p.size == .many)
            "{ " ++ luauType(p.child) ++ " }"
        else if (p.size == .one)
            luauType(p.child)
        else
            @compileError("no Luau type for " ++ @typeName(T)),
        .array => |a| if (a.child == u8) "string" else "{ " ++ luauType(a.child) ++ " }",
        .@"fn" => fnType(T, null),
        .@"struct" => structType(T),
        else => @compileError("no Luau type for " ++ @typeName(T)),
    };
}

/// A `declare <name>: <luauType(T)>` definition line — drop several of these into
/// one string to build a `.d.luau` definition file.
pub fn declsFor(comptime T: type, comptime name: []const u8) []const u8 {
    return comptime "declare " ++ name ++ ": " ++ luauType(T) ++ "\n";
}

fn retType(comptime R: type) []const u8 {
    const P = if (@typeInfo(R) == .error_union) @typeInfo(R).error_union.payload else R;
    return if (P == void) "()" else luauType(P);
}

/// `(p1, p2, …) -> ret`. If `Self` is given, a leading `Self`/`*Self`/`*const Self`
/// parameter is treated as the method receiver and omitted.
fn fnType(comptime F: type, comptime Self: ?type) []const u8 {
    comptime {
        const fi = @typeInfo(F).@"fn";
        var out: []const u8 = "(";
        var first = true;
        for (fi.params, 0..) |p, i| {
            const PT = p.type orelse @compileError("anytype params can't be projected to Luau");
            if (i == 0 and Self != null and
                (PT == Self.? or PT == *Self.? or PT == *const Self.?)) continue;
            if (!first) out = out ++ ", ";
            out = out ++ luauType(PT);
            first = false;
        }
        return out ++ ") -> " ++ retType(fi.return_type.?);
    }
}

fn structType(comptime T: type) []const u8 {
    comptime {
        var out: []const u8 = "{ ";
        var first = true;
        // data fields
        for (@typeInfo(T).@"struct".fields) |f| {
            if (!first) out = out ++ ", ";
            out = out ++ f.name ++ ": " ++ luauType(f.type);
            first = false;
        }
        // public methods (skip lifecycle hooks the host wires itself)
        for (@typeInfo(T).@"struct".decls) |d| {
            const D = @TypeOf(@field(T, d.name));
            if (@typeInfo(D) != .@"fn") continue;
            if (std.mem.eql(u8, d.name, "init") or std.mem.eql(u8, d.name, "deinit")) continue;
            if (!first) out = out ++ ", ";
            out = out ++ d.name ++ ": " ++ fnType(D, T);
            first = false;
        }
        return out ++ " }";
    }
}
