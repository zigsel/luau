//! Tooling: reflect Luau *type annotations* from source — type aliases, table
//! types, unions of string singletons, and function parameter annotations.
//! This is what a custom-config / codegen tool (à la a `ModVisitor`) walks.

const std = @import("std");
const luau = @import("luau");

fn firstOf(p: luau.ast.Parsed, kind: luau.ast.NodeKind) ?luau.ast.Node {
    var i: usize = 0;
    while (i < p.nodeCount()) : (i += 1) {
        const n = p.node(i);
        if (n.kind() == kind) return n;
    }
    return null;
}

pub fn main() !void {
    const source =
        \\type Direction = "north" | "south" | "east" | "west"
        \\type Position = Component<{ x: number, y: number }>
        \\local function move(entity: Entity, dx: number, dy)
        \\end
    ;

    var p = luau.ast.parse(source);
    defer p.deinit();
    if (!p.ok()) return;

    // 1) a type alias whose value is `Component<{ ... }>`
    if (firstOf(p, .stat_type_alias)) |alias| {
        std.debug.print("alias '{s}'", .{alias.typeAliasName()});
        if (alias.typeAliasType()) |t| if (t.kind() == .type_reference) {
            std.debug.print(" = {s}<...>", .{t.typeReferenceName()});
            if (t.typeReferenceParam(0)) |arg| if (arg.kind() == .type_table) {
                std.debug.print(" with fields:", .{});
                var f: usize = 0;
                while (f < arg.typeTablePropCount()) : (f += 1)
                    std.debug.print(" {s}", .{arg.typeTablePropName(f)});
            };
        };
        std.debug.print("\n", .{});
    }

    // 2) a union of string singletons → an enum
    if (firstOf(p, .type_union)) |uni| {
        std.debug.print("enum values:", .{});
        var j: usize = 0;
        while (j < uni.typeUnionCount()) : (j += 1)
            if (uni.typeUnionMember(j)) |m| if (m.kind() == .type_singleton_string)
                std.debug.print(" \"{s}\"", .{m.typeSingletonStringValue()});
        std.debug.print("\n", .{});
    }

    // 3) function parameter annotations (null when a param is unannotated)
    if (firstOf(p, .expr_function)) |fn_node| {
        std.debug.print("params:", .{});
        var k: usize = 0;
        while (k < fn_node.functionParamCount()) : (k += 1) {
            const name = fn_node.functionParamName(k);
            if (fn_node.functionParamAnnotation(k)) |ann|
                std.debug.print(" {s}: {s}", .{ name, ann.typeReferenceName() })
            else
                std.debug.print(" {s}: <none>", .{name});
        }
        std.debug.print("\n", .{});
    }
}
