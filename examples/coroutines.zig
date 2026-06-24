//! Embedding: drive a Luau coroutine from Zig — create a thread, load a
//! function into it, and resume it across its `coroutine.yield` points.

const std = @import("std");
const luau = @import("luau");

pub fn main() !void {
    var vm = try luau.Lua.init(std.heap.page_allocator);
    defer vm.deinit();
    vm.openLibs();

    // A worker that yields three times, then returns a final value.
    const worker =
        \\for i = 1, 3 do
        \\    coroutine.yield(i * 10)
        \\end
        \\return 999
    ;

    // A new thread shares the parent's globals; load the chunk onto its stack.
    const co = vm.newThread();
    try co.loadString("=worker", worker);

    var resumes: usize = 0;
    while (true) {
        const status = try co.resumeThread(vm, 0);
        resumes += 1;
        switch (status) {
            .yield => {
                std.debug.print("  yield #{d}: {d}\n", .{ resumes, co.toNumber(-1).? });
                co.pop(1); // drop the yielded value before resuming again
            },
            .ok => {
                std.debug.print("  finished after {d} resumes, returned {d}\n", .{ resumes, co.toNumber(-1).? });
                break;
            },
        }
    }
}
