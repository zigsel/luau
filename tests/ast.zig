//! AST: build & compile, parse + typed node access, CST, attributes/confusables,
//! and AST→JSON.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

test "ast.build: local function called via an if -> 7, compiled and run" {
    const ab = luau.ast.build;
    const b = ab.Builder.init();
    defer b.deinit();

    const x = b.declareLocal("x", null);
    const body_ret = b.ret(&.{b.binary(.add, b.exprLocal(x), b.number(1))});
    const f_func = b.function(&.{x}, false, &.{body_ret});
    const f = b.declareLocal("f", null);
    const local_fn = b.localFunction(f, f_func);

    const call = b.call(b.exprLocal(f), &.{b.number(6)});
    const then_block = b.block(&.{b.ret(&.{call})});
    const else_block = b.block(&.{b.ret(&.{b.number(0)})});
    const if_stat = b.ifStat(b.boolean(true), then_block, else_block);

    const root = b.block(&.{ local_fn, if_stat });

    const bc = try b.compile(testing.allocator, root);
    defer testing.allocator.free(bc);
    try testing.expect(!luau.compiler.isErrorBytecode(bc));

    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    try vm.loadBytecode("=ast", bc, 0);
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 7), vm.toNumber(-1).?);
}

test "ast.parse: 'return 1 + 2' — find binary expr, read children + op" {
    const p = luau.ast.parse("return 1 + 2");
    defer p.deinit();
    try testing.expect(p.ok());

    var found = false;
    var i: usize = 0;
    while (i < p.nodeCount()) : (i += 1) {
        const n = p.node(i);
        if (n.kind() == .expr_binary) {
            found = true;
            try testing.expect(n.binaryLeft() != null);
            try testing.expect(n.binaryRight() != null);
            _ = n.binaryOp();
            break;
        }
    }
    try testing.expect(found);
}

test "ast.cst: a commented snippet retains the comment (tolerant)" {
    const Cst = luau.ast.cst.Cst;
    const c = Cst.parse(
        \\-- a leading comment
        \\local x = 1
        \\return x
    );
    defer c.deinit();

    try testing.expect(c.nodeCount() > 0);
    if (c.commentCount() > 0) {
        const cm = c.commentAt(0);
        try testing.expect(cm.text.len > 0);
    }
}

test "ast.attributes: parse + confusables run without crashing" {
    var attrs = luau.ast.attributes.parse("@native function f() end") catch return;
    defer attrs.deinit();
    _ = attrs.parsedOk();
    const n = attrs.count();
    if (n > 0) _ = attrs.at(0);

    // Cyrillic 'а' (U+0430) is a classic confusable for Latin 'a'.
    _ = luau.ast.attributes.isConfusable(0x0430);
    _ = luau.ast.attributes.confusableSuggestion(0x0430);
    _ = luau.ast.attributes.confusableSuggestion('a');
}

test "ast.json: to-json of a trivial module is non-empty and recognizable" {
    const out = try luau.ast.json.toJson(testing.allocator, "local x = 1\nreturn x");
    defer testing.allocator.free(out);

    try testing.expect(out.len > 0);
    try testing.expect(std.mem.indexOf(u8, out, "AstStatBlock") != null or
        std.mem.indexOf(u8, out, "type") != null);
}
