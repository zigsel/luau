//! `.luaurc` path-alias parsing and case-insensitive resolution — now part of
//! the unified `luau.config` (one `.luaurc` parse exposes mode, globals, lint,
//! AND aliases with case-folded keys + resolution).

const std = @import("std");
const testing = std.testing;
const luau = @import("luau");

const config = luau.config;

const luaurc =
    \\{
    \\  "aliases": {
    \\    "Lib": "./src/lib",
    \\    "vendor": "./third_party"
    \\  }
    \\}
;

test "config: parse and enumerate aliases" {
    var cfg = config.parse(luaurc);
    defer cfg.deinit();

    try testing.expect(cfg.ok());
    try testing.expectEqual(@as(usize, 2), cfg.aliasCount());

    // Find the "Lib" alias by its original case among the entries.
    var found_lib = false;
    var it = cfg.aliases();
    while (it.next()) |entry| {
        if (std.mem.eql(u8, entry.name, "Lib")) {
            found_lib = true;
            try testing.expectEqualStrings("lib", entry.key); // case-folded
            try testing.expectEqualStrings("./src/lib", entry.value);
        }
    }
    try testing.expect(found_lib);
}

test "config: case-insensitive alias resolution" {
    var cfg = config.parse(luaurc);
    defer cfg.deinit();
    try testing.expect(cfg.ok());

    // Resolves regardless of case (Luau lowercases alias keys).
    try testing.expectEqualStrings("./src/lib", cfg.resolveAlias("Lib").?);
    try testing.expectEqualStrings("./src/lib", cfg.resolveAlias("lib").?);
    try testing.expectEqualStrings("./src/lib", cfg.resolveAlias("LIB").?);
    try testing.expectEqualStrings("./third_party", cfg.resolveAlias("vendor").?);

    try testing.expect(cfg.resolveAlias("missing") == null);
}

test "config: alias validity check" {
    try testing.expect(config.isValidAlias("lib"));
    try testing.expect(config.isValidAlias("@lib"));
    try testing.expect(!config.isValidAlias("not/valid"));
}
