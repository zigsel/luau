//! Tooling: parse a `.luaurc`, read its settings, and resolve path aliases —
//! the kind of thing a require-resolver or project tool needs.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    const luaurc =
        \\{
        \\  "languageMode": "strict",
        \\  "globals": ["describe", "it"],
        \\  "aliases": {
        \\    "Lib": "./src/lib",
        \\    "vendor": "./third_party"
        \\  }
        \\}
    ;

    var cfg = luau.config.parse(luaurc);
    defer cfg.deinit();

    if (!cfg.ok()) {
        std.debug.print("invalid .luaurc: {s}\n", .{cfg.err() orelse "?"});
        return;
    }

    std.debug.print("mode: {s}\n", .{@tagName(cfg.mode())});

    std.debug.print("globals:", .{});
    var g: usize = 0;
    while (g < cfg.globalCount()) : (g += 1) std.debug.print(" {s}", .{cfg.global(g)});
    std.debug.print("\n", .{});

    std.debug.print("aliases:\n", .{});
    var it = cfg.aliases();
    while (it.next()) |a|
        std.debug.print("  {s} (key '{s}') -> {s}\n", .{ a.name, a.key, a.value });

    // Case-insensitive resolution — what `require("@Lib/foo")` needs for `@Lib`.
    std.debug.print("resolve 'lib'    -> {s}\n", .{cfg.resolveAlias("lib") orelse "(none)"});
    std.debug.print("resolve 'VENDOR' -> {s}\n", .{cfg.resolveAlias("VENDOR") orelse "(none)"});
    std.debug.print("resolve 'nope'   -> {s}\n", .{cfg.resolveAlias("nope") orelse "(none)"});
}
