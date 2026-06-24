//! Tests for the signature/documentation database (signatures.zig): the richer
//! `Documentation.h` variants behind signature help and structured hover. Pure
//! in-memory data — we populate the database and read it back.

const std = @import("std");
const luau = @import("luau");

const signatures = luau.analysis.signatures;
const testing = std.testing;

test "signatures: function with parameters and returns" {
    var db = signatures.Database.init() catch return;
    defer db.deinit();

    try db.addFunction("@roblox/print", "Prints values.", "https://example/print", "print(1)");
    try db.addParameter("@roblox/print", "value", "@roblox/print/param/value");
    try db.addParameter("@roblox/print", "more", "");
    try db.addReturn("@roblox/print", "@roblox/print/ret/0");

    try testing.expect(db.isFunction("@roblox/print"));
    try testing.expect(!db.isTable("@roblox/print"));
    try testing.expect(!db.isOverloaded("@roblox/print"));
    try testing.expectEqual(@as(usize, 1), db.count());

    try testing.expectEqualStrings("Prints values.", db.functionDocumentation("@roblox/print").?);
    try testing.expectEqualStrings("https://example/print", db.functionLearnMore("@roblox/print").?);
    try testing.expectEqualStrings("print(1)", db.functionCodeSample("@roblox/print").?);

    try testing.expectEqual(@as(usize, 2), db.parameterCount("@roblox/print"));
    try testing.expectEqual(@as(usize, 1), db.returnCount("@roblox/print"));

    const p0 = db.parameter("@roblox/print", 0).?;
    try testing.expectEqualStrings("value", p0.name);
    try testing.expectEqualStrings("@roblox/print/param/value", p0.doc_symbol);

    const p1 = db.parameter("@roblox/print", 1).?;
    try testing.expectEqualStrings("more", p1.name);
    try testing.expectEqualStrings("", p1.doc_symbol);

    try testing.expect(db.parameter("@roblox/print", 2) == null);
    try testing.expectEqualStrings("@roblox/print/ret/0", db.returnSymbol("@roblox/print", 0).?);
    try testing.expect(db.returnSymbol("@roblox/print", 1) == null);

    // iterator visits both parameters in order
    var it = db.parameters("@roblox/print");
    var names: std.ArrayList([]const u8) = .empty;
    defer names.deinit(testing.allocator);
    while (it.next()) |p| try names.append(testing.allocator, p.name);
    try testing.expectEqual(@as(usize, 2), names.items.len);
    try testing.expectEqualStrings("value", names.items[0]);
    try testing.expectEqualStrings("more", names.items[1]);
}

test "signatures: table keys" {
    var db = signatures.Database.init() catch return;
    defer db.deinit();

    try db.addTable("@roblox/Instance", "A game object.", "", "");
    try db.addKey("@roblox/Instance", "Name", "@roblox/Instance/Name");
    try db.addKey("@roblox/Instance", "Parent", "@roblox/Instance/Parent");

    try testing.expect(db.isTable("@roblox/Instance"));
    try testing.expect(!db.isFunction("@roblox/Instance"));
    try testing.expectEqualStrings("A game object.", db.tableDocumentation("@roblox/Instance").?);
    try testing.expect(db.tableLearnMore("@roblox/Instance") != null);
    try testing.expect(db.tableCodeSample("@roblox/Instance") != null);

    try testing.expectEqualStrings("@roblox/Instance/Name", db.keyDoc("@roblox/Instance", "Name").?);
    try testing.expectEqualStrings("@roblox/Instance/Parent", db.keyDoc("@roblox/Instance", "Parent").?);
    try testing.expect(db.keyDoc("@roblox/Instance", "Missing") == null);
}

test "signatures: overloaded function" {
    var db = signatures.Database.init() catch return;
    defer db.deinit();

    try db.addOverloaded("@roblox/typeof");
    try db.addOverload("@roblox/typeof", "(x: any) -> string", "@roblox/typeof/0");

    try testing.expect(db.isOverloaded("@roblox/typeof"));
    try testing.expect(!db.isFunction("@roblox/typeof"));
    try testing.expectEqualStrings("@roblox/typeof/0", db.overloadDoc("@roblox/typeof", "(x: any) -> string").?);
    try testing.expect(db.overloadDoc("@roblox/typeof", "(x: number) -> string") == null);
}

test "signatures: wrong-variant accessors return null" {
    var db = signatures.Database.init() catch return;
    defer db.deinit();

    try db.addFunction("@x/f", "", "", "");
    try testing.expect(db.tableDocumentation("@x/f") == null);
    try testing.expect(db.overloadDoc("@x/f", "sig") == null);
    try testing.expect(db.functionDocumentation("@x/missing") == null);
    try testing.expectError(error.AddFailed, db.addParameter("@x/missing", "p", ""));
    try testing.expectError(error.AddFailed, db.addKey("@x/f", "k", ""));
}
