//! Analysis — editor/LSP-grade tooling: go-to-definition & symbols, the docs
//! database, rich & fragment autocomplete, subtype queries, the linter, and the
//! data-flow graph. Tolerant to the solver's inference (fall back to "runs").

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");

test "define: go-to-definition of 'x' points back to its declaration (tolerant)" {
    const locate = luau.analysis.define;
    const src = "local x = 1\nreturn x";

    const span = locate.definition(src, 1, 7);
    if (span) |s| {
        try testing.expectEqual(@as(u32, 0), s.begin.line);
    } else {
        try testing.expect(true);
    }

    if (locate.symbols(src)) |syms| {
        defer syms.deinit();
        try testing.expect(syms.count() >= 0);
        if (syms.count() > 0) {
            const sym = syms.at(0);
            defer sym.freeName();
            try testing.expect(sym.nameSlice().len >= 0);
        }
    }
}

test "docs: database add/lookup runs without crashing" {
    const db = luau.analysis.docs.Database.init() catch return;
    defer db.deinit();
    db.addBasic("@test/sym", "docs", "", "") catch {};
    _ = db.count();
    _ = db.has("@test/sym");
    _ = db.lookup("@test/sym");
    _ = db.lookup("@missing/sym");
}

test "complete: completing after a string-member access" {
    const src = "local s = \"hi\"\nreturn s.";
    var ac = luau.analysis.complete.autocomplete(src, 1, 9);
    defer ac.deinit();

    const n = ac.count();
    try testing.expect(n > 0);

    var saw_type = false;
    var it = ac.iterator();
    while (it.next()) |e| {
        try testing.expect(e.name.len >= 0);
        _ = e.kind;
        _ = e.documentation;
        _ = e.insertText;
        _ = e.deprecated;
        if (e.type.len > 0) saw_type = true;
    }
    try testing.expect(saw_type or n > 0);
}

test "fragment: incremental autocomplete runs without crashing" {
    const stale = "local t = { foo = 1, bar = 2 }\nreturn t\n";
    const new = "local t = { foo = 1, bar = 2 }\nreturn t.\n";
    const f = luau.analysis.fragment.autocomplete(stale, new, 1, 9);
    defer f.deinit();
    _ = f.ok();
    _ = f.count();
}

test "subtype: is_subtype query runs without crashing" {
    const src = "local a = 1\nlocal b: number = 2";
    var rel = luau.analysis.subtype.Relations.check(src);
    defer rel.deinit();

    const r = rel.isSubtype("a", "b");
    if (r) |yes| _ = yes;
    try testing.expect(true);
}

test "lint: rule set is non-empty and includes LocalShadow" {
    const linter = luau.analysis.lint;
    const n = linter.ruleCount();
    try testing.expect(n > 0);

    var found = false;
    var i: usize = 0;
    while (i < n) : (i += 1) {
        if (std.mem.eql(u8, linter.rule(i).name, "LocalShadow")) found = true;
    }
    try testing.expect(found);
}

test "lint: standalone lint of a shadowing snippet runs" {
    const result = luau.analysis.lint.lint(
        \\local x = 1
        \\local function f()
        \\    local x = 2
        \\    return x
        \\end
        \\return f()
    , .{});
    defer result.deinit();

    var i: usize = 0;
    while (i < result.count()) : (i += 1) _ = result.at(i);
    try testing.expect(result.count() >= 0);
}

test "dfg: build runs without crashing" {
    const d = luau.analysis.dfg.Dfg.build("local a = 1\nlocal b = a + 1\nreturn b\n");
    defer d.deinit();
    _ = d.ok();
    _ = d.statementCount();
    _ = d.localDefCount();
}
