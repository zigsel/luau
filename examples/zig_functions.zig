//! Expose Zig functions to Luau by their real signatures. Arguments are pulled
//! from the stack and results pushed automatically; a returned `error` is raised
//! as a Luau error.

const std = @import("std");
const luau = @import("luau");

fn add(a: f64, b: f64) f64 {
    return a + b;
}

fn greet(name: []const u8) []const u8 {
    // (returns a borrowed slice; copied into the VM on push)
    return if (name.len == 0) "hello, stranger" else "hello!";
}

fn parseInt(s: []const u8) !i32 {
    return std.fmt.parseInt(i32, s, 10); // error -> Luau error
}

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    vm.setFn("add", add);
    vm.setFn("greet", greet);
    vm.setFn("parseInt", parseInt);

    try vm.doString("=demo",
        \\ print(add(40, 2))            -- 42
        \\ print(greet("world"))        -- hello!
        \\ print(parseInt("123") + 1)   -- 124
        \\
        \\ -- the Zig error surfaces as a catchable Luau error
        \\ local ok, err = pcall(parseInt, "not a number")
        \\ print(ok, err)               -- false  ...InvalidCharacter
    );
}
