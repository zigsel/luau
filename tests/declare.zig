//! Projecting Zig host types into Luau type definitions, then type-checking a
//! mod script against them.

const std = @import("std");
const luau = @import("luau");
const testing = std.testing;

const Vec2 = struct { x: f64, y: f64 };

const GameWorld = struct {
    tick: f64,
    gravity: Vec2,
    names: []const []const u8,

    pub fn init() GameWorld { // skipped in the projection
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

test "luauType renders Zig types as Luau type expressions" {
    try testing.expectEqualStrings("number", luau.luauType(f64));
    try testing.expectEqualStrings("string", luau.luauType([]const u8));
    try testing.expectEqualStrings("boolean", luau.luauType(bool));
    try testing.expectEqualStrings("number?", luau.luauType(?i32));
    try testing.expectEqualStrings("{ number }", luau.luauType([]const f64));
    try testing.expectEqualStrings("{ x: number, y: number }", luau.luauType(Vec2));
}

test "declsFor projects a host struct (fields + methods, skipping init)" {
    const defs = luau.declsFor(GameWorld, "world");
    // fields then methods; `spawn` drops nothing (no self), `paused` -> () -> boolean.
    try testing.expectEqualStrings(
        "declare world: { tick: number, gravity: { x: number, y: number }, " ++
            "names: { string }, spawn: (string, { x: number, y: number }) -> number, " ++
            "paused: () -> boolean }\n",
        defs,
    );
}

test "a mod type-checks against the generated host definitions" {
    const defs = luau.declsFor(GameWorld, "world");

    // correct usage — no type errors
    {
        var r = luau.analysis.defs.checkWithDefinitions(defs,
            \\local id = world.spawn("goblin", { x = 1, y = 2 })
            \\return id + world.tick
        );
        defer r.deinit();
        try testing.expect(r.defsOk()); // the definition file itself parsed
        try testing.expect(r.ok()); // and the script checked clean
    }

    // misuse — calling a field that doesn't exist must be a type error
    {
        var r = luau.analysis.defs.checkWithDefinitions(defs,
            \\return world.nonexistent()
        );
        defer r.deinit();
        try testing.expect(r.defsOk());
        try testing.expect(!r.ok()); // the checker rejects it
    }
}
