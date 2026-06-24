//! The C++ tooling allocates through a host-supplied allocator (global override
//! of `operator new`/`delete`).

const std = @import("std");
const luau = @import("luau");
const testing = std.testing;

/// Counts bytes/allocations passing through it; backs onto the page allocator.
const Counting = struct {
    backing: std.mem.Allocator = std.heap.page_allocator,
    bytes: usize = 0,
    allocs: usize = 0,
    live: usize = 0,

    fn allocator(self: *Counting) std.mem.Allocator {
        return .{ .ptr = self, .vtable = &.{ .alloc = alloc, .resize = resize, .remap = remap, .free = free } };
    }
    fn alloc(ctx: *anyopaque, len: usize, a: std.mem.Alignment, ra: usize) ?[*]u8 {
        const self: *Counting = @ptrCast(@alignCast(ctx));
        const p = self.backing.vtable.alloc(self.backing.ptr, len, a, ra) orelse return null;
        self.bytes += len;
        self.allocs += 1;
        self.live += 1;
        return p;
    }
    fn free(ctx: *anyopaque, mem: []u8, a: std.mem.Alignment, ra: usize) void {
        const self: *Counting = @ptrCast(@alignCast(ctx));
        self.live -= 1;
        self.backing.vtable.free(self.backing.ptr, mem, a, ra);
    }
    fn resize(ctx: *anyopaque, mem: []u8, a: std.mem.Alignment, n: usize, ra: usize) bool {
        const self: *Counting = @ptrCast(@alignCast(ctx));
        return self.backing.vtable.resize(self.backing.ptr, mem, a, n, ra);
    }
    fn remap(ctx: *anyopaque, mem: []u8, a: std.mem.Alignment, n: usize, ra: usize) ?[*]u8 {
        const self: *Counting = @ptrCast(@alignCast(ctx));
        return self.backing.vtable.remap(self.backing.ptr, mem, a, n, ra);
    }
};

test "C++ tooling allocations flow through a custom allocator" {
    var counting = Counting{};
    luau.setAllocator(counting.allocator());
    defer luau.useDefaultAllocator(); // critical: stop using the stack value

    const before = counting.allocs;

    // Exercise the tooling: parse + type-check allocate plenty of C++ objects.
    {
        var p = luau.ast.parse(
            \\type T = { x: number, y: string }
            \\local function f(a: number) return a * 2 end
            \\return f(21)
        );
        defer p.deinit();
        try testing.expect(p.ok());

        var checked = luau.analysis.check("local x: number = 1 return x + 1");
        defer checked.deinit();
    }

    // The custom allocator must have serviced the tooling's operator-new traffic.
    try testing.expect(counting.allocs > before);
    try testing.expect(counting.bytes > 0);
}
