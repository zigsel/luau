//! Source-level type-annotation reflection — the AST type-node accessors.
//! Mirrors a real-world `ModVisitor`: type aliases with table generics
//! (`Component<{...}>`), unions of string singletons (enums), and annotated
//! function parameters.

const std = @import("std");
const luau = @import("luau");

const ast = luau.ast;

/// Find the first node of a given kind (pre-order).
fn firstOf(p: ast.Parsed, kind: ast.NodeKind) ?ast.Node {
    var i: usize = 0;
    while (i < p.nodeCount()) : (i += 1) {
        const n = p.node(i);
        if (n.kind() == kind) return n;
    }
    return null;
}

test "type alias with a table generic argument (Component<{...}>)" {
    const src =
        \\type Position = Component<{ x: number, y: number }>
    ;
    var p = ast.parse(src);
    defer p.deinit();
    try std.testing.expect(p.ok());

    const alias = firstOf(p, .stat_type_alias).?;
    try std.testing.expectEqualStrings("Position", alias.typeAliasName());

    // alias.type is `Component<{...}>` — a type reference with one type argument
    const ref = alias.typeAliasType().?;
    try std.testing.expectEqual(ast.NodeKind.type_reference, ref.kind());
    try std.testing.expectEqualStrings("Component", ref.typeReferenceName());
    try std.testing.expectEqual(@as(usize, 1), ref.typeReferenceParamCount());

    // the argument is `{ x: number, y: number }`
    const table = ref.typeReferenceParam(0).?;
    try std.testing.expectEqual(ast.NodeKind.type_table, table.kind());
    try std.testing.expectEqual(@as(usize, 2), table.typeTablePropCount());
    try std.testing.expectEqualStrings("x", table.typeTablePropName(0));
    try std.testing.expectEqualStrings("number", table.typeTablePropType(0).?.typeReferenceName());
    try std.testing.expectEqualStrings("y", table.typeTablePropName(1));
}

test "union of string singletons (enum) and an optional member" {
    const src =
        \\type Dir = "north" | "south" | "east" | nil
    ;
    var p = ast.parse(src);
    defer p.deinit();
    try std.testing.expect(p.ok());

    const uni = firstOf(p, .type_union).?;
    var values: std.ArrayList([]const u8) = .empty;
    defer values.deinit(std.testing.allocator);

    var j: usize = 0;
    while (j < uni.typeUnionCount()) : (j += 1) {
        const m = uni.typeUnionMember(j).?;
        switch (m.kind()) {
            .type_singleton_string => try values.append(std.testing.allocator, m.typeSingletonStringValue()),
            else => {}, // `nil` shows up as a type_reference named "nil"
        }
    }
    try std.testing.expectEqual(@as(usize, 3), values.items.len);
    try std.testing.expectEqualStrings("north", values.items[0]);
    try std.testing.expectEqualStrings("east", values.items[2]);
}

test "function parameter annotations" {
    const src =
        \\local function move(entity: Entity, dx: number, dy)
        \\end
    ;
    var p = ast.parse(src);
    defer p.deinit();
    try std.testing.expect(p.ok());

    const fn_node = firstOf(p, .expr_function).?;
    try std.testing.expectEqual(@as(usize, 3), fn_node.functionParamCount());

    // entity: Entity
    try std.testing.expectEqualStrings("entity", fn_node.functionParamName(0));
    try std.testing.expectEqualStrings("Entity", fn_node.functionParamAnnotation(0).?.typeReferenceName());
    // dx: number
    try std.testing.expectEqualStrings("number", fn_node.functionParamAnnotation(1).?.typeReferenceName());
    // dy — unannotated
    try std.testing.expect(fn_node.functionParamAnnotation(2) == null);
}
