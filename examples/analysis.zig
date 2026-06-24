//! Tooling: type-check Luau source, surface lint warnings, and autocomplete.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    // 1) type checking
    {
        var r = luau.analysis.check("local x: number = \"oops\"\nreturn x\n");
        defer r.deinit();
        std.debug.print("type errors: {d}\n", .{r.errorCount()});
        var it = r.errors();
        while (it.next()) |e|
            std.debug.print("  @ {d}:{d}: {s}\n", .{ e.position.line, e.position.column, e.message });
    }

    // 2) linting (a shadowed local)
    {
        var r = luau.analysis.check(
            \\local x = 1
            \\do local x = 2; print(x) end
            \\return x
        );
        defer r.deinit();
        var it = r.lints();
        while (it.next()) |w|
            std.debug.print("lint [{s}] @ {d}:{d}: {s}\n", .{ w.name, w.position.line, w.position.column, w.message });
    }

    // 3) autocomplete after a dot
    {
        var ac = luau.analysis.autocomplete("local s = \"hi\"\nreturn s.\n", 1, 9);
        defer ac.deinit();
        std.debug.print("autocomplete: {d} suggestions, has 'upper'={}\n", .{ ac.count(), ac.contains("upper") });
    }
}
