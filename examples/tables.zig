//! Working with tables from Zig: the `Table` handle (get/set/len/iterate) and a
//! typed `Ref` to pin a value across stack churn.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();

    var vm = try luau.Lua.init(gpa.allocator());
    defer vm.deinit();
    vm.openLibs();

    // build a table from Zig
    var t = luau.Table.init(vm);
    defer t.deinit();
    t.set("host", "localhost");
    t.set("port", @as(i32, 8080));
    t.set("tls", true);

    std.debug.print("host={s} port={d} tls={}\n", .{
        try t.get([]const u8, "host"),
        try t.get(i32, "port"),
        try t.get(bool, "tls"),
    });

    // iterate key/value pairs
    var it = t.iterator();
    var n: usize = 0;
    while (it.next()) {
        n += 1;
        it.step();
    }
    std.debug.print("entries: {d}\n", .{n});

    // a Ref keeps a value alive regardless of the stack
    vm.pushString("kept");
    const ref = luau.Ref([]const u8).pop(vm);
    defer ref.deinit();
    vm.setTop(0); // wipe the stack
    std.debug.print("ref still holds: {s}\n", .{try ref.get()});
}
