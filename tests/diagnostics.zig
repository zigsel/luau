const std = @import("std");
const luau = @import("luau");
const diagnostics = luau.analysis.diagnostics;

test "unknown global yields unknown_symbol with name and location" {
    // `nonexistentGlobal` is an unknown binding on line 1.
    const result = diagnostics.check("local x = nonexistentGlobal\n");
    defer result.deinit();

    try std.testing.expect(!result.ok());
    try std.testing.expect(result.count() >= 1);

    var found = false;
    var it = result.iterator();
    while (it.next()) |d| {
        if (d.kind == .unknown_symbol) {
            found = true;
            // The cheap typed field is the symbol name.
            try std.testing.expect(d.field != null);
            try std.testing.expectEqualStrings("nonexistentGlobal", d.field.?);
            // Reported on the first line.
            try std.testing.expectEqual(@as(u32, 0), d.location.begin.line);
            try std.testing.expect(d.message.len > 0);
        }
    }
    try std.testing.expect(found);
}

test "clean module produces no diagnostics" {
    const result = diagnostics.check("local x = 1\nreturn x\n");
    defer result.deinit();
    try std.testing.expect(result.ok());
    try std.testing.expectEqual(@as(usize, 0), result.count());
}
