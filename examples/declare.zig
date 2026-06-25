//! Tooling: project a Zig host API into Luau type definitions (`declsFor`),
//! print them, and type-check a mod script against them. This is the file you'd
//! write to `types/host.d.luau` for `luau-lsp` editor autocomplete.

const std = @import("std");
const luau = @import("luau");

const Vec2 = struct { x: f64, y: f64 };

const GameWorld = struct {
    tick: f64,
    gravity: Vec2,
    names: []const []const u8,

    pub fn init() GameWorld { // lifecycle hook — skipped in the projection
        return undefined;
    }
    pub fn spawn(name: []const u8, pos: Vec2) u32 {
        _ = name;
        _ = pos;
        return 0;
    }
    pub fn paused() bool {
        return false;
    }
};

pub fn main() !void {
    // 1) project the Zig struct into Luau definitions (comptime, no allocation).
    const defs = luau.declsFor(GameWorld, "world");
    std.debug.print("--- generated Luau definitions (types/host.d.luau) ---\n{s}\n", .{defs});

    // 2) a mod that uses the API correctly type-checks clean.
    {
        var r = luau.analysis.defs.checkWithDefinitions(defs,
            \\local id = world.spawn("goblin", { x = 1, y = 2 })
            \\return id + world.tick
        );
        defer r.deinit();
        std.debug.print("correct mod  -> ok={}\n", .{r.ok()});
    }

    // 3) a mod that misuses it is rejected, with the checker's diagnostics.
    {
        var r = luau.analysis.defs.checkWithDefinitions(defs, "return world.nonexistent()");
        defer r.deinit();
        std.debug.print("broken mod   -> ok={}\n", .{r.ok()});
        var it = r.errors();
        while (it.next()) |e|
            std.debug.print("  {d}:{d}  {s}\n", .{ e.position.line, e.position.column, e.message });
    }
}
