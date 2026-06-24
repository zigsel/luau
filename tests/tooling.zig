//! The Luau language tooling: parsing (AST), `.luaurc` config, and analysis
//! (type checking, linting, autocomplete).

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");

test "ast: parse valid and invalid Luau" {
    {
        var p = luau.ast.parse("local x = 1\nreturn x + 2\n");
        defer p.deinit();
        try testing.expect(p.ok());
        try testing.expect(p.hasRoot());
        try testing.expectEqual(@as(usize, 0), p.errorCount());
    }
    {
        var p = luau.ast.parse("local = = bad");
        defer p.deinit();
        try testing.expect(!p.ok());
        try testing.expect(p.errorCount() > 0);
        try testing.expect(p.getError(0).message.len > 0);
    }
}

test "ast: walk the node tree" {
    var p = luau.ast.parse("local x = 42\nreturn x + 1\n");
    defer p.deinit();
    try testing.expect(p.ok());

    const root = p.root().?;
    try testing.expectEqual(luau.ast.NodeKind.stat_block, root.kind());

    var found_local = false;
    var found_number = false;
    var i: usize = 0;
    while (i < p.nodeCount()) : (i += 1) {
        const n = p.node(i);
        switch (n.kind()) {
            .stat_local => {
                found_local = true;
                try testing.expectEqualStrings("x", n.string());
            },
            .expr_constant_number => if (n.number() == 42) {
                found_number = true;
            },
            .expr_binary => try testing.expectEqual(luau.ast.BinaryOp.add, n.binaryOp()),
            else => {},
        }
    }
    try testing.expect(found_local and found_number);

    var nchildren: usize = 0;
    var it = root.children();
    while (it.next()) |_| nchildren += 1;
    try testing.expect(nchildren >= 2);
}

test "ast: hot comments" {
    var p = luau.ast.parse("--!strict\nlocal x = 1\n");
    defer p.deinit();
    try testing.expectEqual(@as(usize, 1), p.hotcommentCount());
    try testing.expect(std.mem.indexOf(u8, p.hotcomment(0), "strict") != null);
}

test "config: parse a .luaurc" {
    var cfg = luau.config.parse(
        \\{ "languageMode": "strict", "globals": ["foo", "bar"] }
    );
    defer cfg.deinit();
    try testing.expect(cfg.ok());
    try testing.expectEqual(luau.config.Mode.strict, cfg.mode());
    try testing.expectEqual(@as(usize, 2), cfg.globalCount());
    try testing.expectEqualStrings("foo", cfg.global(0));
}

test "analysis: type checking" {
    {
        var r = luau.analysis.check("local x: number = 1\nreturn x + 2\n");
        defer r.deinit();
        try testing.expect(r.ok());
    }
    {
        var r = luau.analysis.check("local x: number = \"not a number\"\n");
        defer r.deinit();
        try testing.expect(!r.ok());
        try testing.expect(r.getError(0).message.len > 0);
    }
}

test "analysis: lint warnings" {
    var r = luau.analysis.check(
        \\local x = 1
        \\local function f()
        \\    local x = 2
        \\    return x
        \\end
        \\return f()
    );
    defer r.deinit();
    var saw_lint = false;
    var it = r.lints();
    while (it.next()) |w| {
        try testing.expect(w.message.len > 0 and w.name.len > 0);
        saw_lint = true;
    }
    try testing.expect(saw_lint);
}

test "analysis: autocomplete" {
    var ac = luau.analysis.autocomplete("local s = \"hi\"\nreturn s.\n", 1, 9);
    defer ac.deinit();
    try testing.expect(ac.count() > 0);
}
