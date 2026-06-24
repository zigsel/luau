//! Tooling: parse Luau source into an AST and walk the node tree.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    const source =
        \\local function greet(name)
        \\    return "hi " .. name
        \\end
        \\return greet("world")
    ;

    var p = luau.ast.parse(source);
    defer p.deinit();

    if (!p.ok()) {
        var it = p.errors();
        while (it.next()) |e|
            std.debug.print("syntax error @ {d}:{d}: {s}\n", .{ e.position.line, e.position.column, e.message });
        return;
    }

    std.debug.print("parsed {d} nodes across {d} lines\n", .{ p.nodeCount(), p.lineCount() });

    // visit every node, printing identifiers and string literals
    var i: usize = 0;
    while (i < p.nodeCount()) : (i += 1) {
        const n = p.node(i);
        switch (n.kind()) {
            .stat_local, .stat_local_function => std.debug.print("  declares: {s}\n", .{n.string()}),
            .expr_constant_string => std.debug.print("  string:   \"{s}\"\n", .{n.string()}),
            .expr_call => std.debug.print("  a call at line {d}\n", .{n.begin().line}),
            else => {},
        }
    }
}
