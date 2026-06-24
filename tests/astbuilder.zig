//! Focused tests for the freshly generated shims: the AST builder + compile
//! path, additional BytecodeBuilder methods, and `.luaurc` alias parsing.

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");
const Lua = luau.Lua;

test "ast builder: build 'return 1 + 2', compile, load+run -> 3" {
    const ab = luau.ast.build;
    const b = ab.Builder.init();
    defer b.deinit();

    const one = b.number(1);
    const two = b.number(2);
    const sum = b.binary(.add, one, two);
    const ret = b.ret(&.{sum});
    const root = b.block(&.{ret});

    const bc = try b.compile(testing.allocator, root);
    defer testing.allocator.free(bc);
    try testing.expect(!luau.compiler.isErrorBytecode(bc));

    const vm = try Lua.init(testing.allocator);
    defer vm.deinit();
    vm.openLibs();
    try vm.loadBytecode("=astbuild", bc, 0);
    try vm.pcall(0, 1, 0);
    try testing.expectEqual(@as(f64, 3), vm.toNumber(-1).?);
}

test "bytecode builder: constants, counts, finalize" {
    const b = luau.bytecode.emit.Builder.init();
    defer b.deinit();

    const fid = b.beginFunction(0, false);
    const ki = b.addConstantInteger(7);
    const ks = b.addConstantString("hello");
    const kn = b.addConstantNumber(1.5);
    try testing.expect(ki >= 0 and ks >= 0 and kn >= 0);

    b.setDebugFunctionName("main");
    b.emitAD(@intFromEnum(luau.bytecode.emit.Op.loadn), 0, 7);
    b.emitABC(@intFromEnum(luau.bytecode.emit.Op.@"return"), 0, 2, 0);
    try testing.expect(b.instructionCount() >= 2);

    b.endFunction(1, 0);
    b.setMainFunction(fid);
    b.finalize();
    try testing.expect(b.bytecode().len > 0);

    // static helpers
    try testing.expect(luau.bytecode.emit.getVersion() > 0);
    try testing.expect(luau.bytecode.emit.getStringHash("hello") != 0);
}

test "config: parse a .luaurc with aliases and read them back" {
    var cfg = luau.config.parse(
        \\{ "aliases": { "lib": "./src/lib", "vendor": "./third_party" } }
    );
    defer cfg.deinit();
    try testing.expect(cfg.ok());
    try testing.expectEqual(@as(usize, 2), cfg.aliasCount());

    var found_lib = false;
    var found_vendor = false;
    var it = cfg.aliases();
    while (it.next()) |a| {
        if (std.mem.eql(u8, a.name, "lib")) {
            found_lib = true;
            try testing.expectEqualStrings("./src/lib", a.value);
        } else if (std.mem.eql(u8, a.name, "vendor")) {
            found_vendor = true;
            try testing.expectEqualStrings("./third_party", a.value);
        }
    }
    try testing.expect(found_lib and found_vendor);
}
