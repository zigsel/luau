//! Tooling: install a configurable `require` by implementing the idiomatic
//! `Resolver` vtable — here over an in-memory virtual module tree. No raw C
//! `Configuration`, function pointers, or `*anyopaque` in sight.

const std = @import("std");
const luau = @import("luau");
const Lua = luau.Lua;
const req = luau.require;

/// A flat in-memory module set; navigation builds a `/`-joined path in `buf`.
const Vfs = struct {
    names: []const []const u8,
    sources: []const []const u8,
    buf: [256]u8 = undefined,
    cur: []const u8 = "",

    fn self(ctx: *anyopaque) *Vfs {
        return @ptrCast(@alignCast(ctx));
    }
    fn set(v: *Vfs, s: []const u8) void {
        std.mem.copyForwards(u8, v.buf[0..s.len], s); // s may alias v.buf
        v.cur = v.buf[0..s.len];
    }
    fn find(v: *Vfs, name: []const u8) ?usize {
        for (v.names, 0..) |n, i| if (std.mem.eql(u8, n, name)) return i;
        return null;
    }

    fn isRequireAllowed(_: *anyopaque, _: *Lua, _: []const u8) bool {
        return true;
    }
    fn reset(ctx: *anyopaque, _: *Lua, requirer_chunkname: []const u8) req.NavigateResult {
        // Point at the requirer so `toParent` yields its directory.
        var nm = requirer_chunkname;
        if (nm.len > 0 and (nm[0] == '=' or nm[0] == '@')) nm = nm[1..];
        self(ctx).set(nm);
        return .success;
    }
    fn toParent(ctx: *anyopaque, _: *Lua) req.NavigateResult {
        const v = self(ctx);
        if (v.cur.len == 0) return .not_found; // at root — stops the config walk
        if (std.mem.lastIndexOfScalar(u8, v.cur, '/')) |i| v.set(v.cur[0..i]) else v.set("");
        return .success;
    }
    fn toChild(ctx: *anyopaque, _: *Lua, name: []const u8) req.NavigateResult {
        const v = self(ctx);
        if (std.mem.eql(u8, name, ".")) return .success; // "./x" — stay put
        if (v.cur.len == 0) {
            v.set(name);
        } else {
            var tmp: [256]u8 = undefined;
            v.set(std.fmt.bufPrint(&tmp, "{s}/{s}", .{ v.cur, name }) catch return .not_found);
        }
        return .success;
    }
    fn isModulePresent(ctx: *anyopaque, _: *Lua) bool {
        const v = self(ctx);
        return v.find(v.cur) != null;
    }
    fn curName(ctx: *anyopaque, _: *Lua) []const u8 {
        return self(ctx).cur;
    }
    fn configStatus(_: *anyopaque, _: *Lua) req.ConfigStatus {
        return .absent;
    }
    fn load(ctx: *anyopaque, lua: *Lua, _: []const u8, _: []const u8, loadname: []const u8) i32 {
        const v = self(ctx);
        const i = v.find(loadname) orelse return 0;
        lua.loadString("=module", v.sources[i]) catch return 0;
        lua.pcall(0, 1, 0) catch return 0;
        return 1;
    }

    const vtable = req.Resolver.VTable{
        .isRequireAllowed = isRequireAllowed,
        .reset = reset,
        .toParent = toParent,
        .toChild = toChild,
        .isModulePresent = isModulePresent,
        .getChunkname = curName,
        .getLoadname = curName,
        .getCacheKey = curName,
        .getConfigStatus = configStatus,
        .load = load,
    };
};

pub fn main() !void {
    var vfs = Vfs{
        .names = &.{ "math", "greeting" },
        .sources = &.{
            "return { add = function(a, b) return a + b end }",
            "return 'hello'",
        },
    };
    var resolver = req.Resolver{ .ptr = &vfs, .vtable = &Vfs.vtable };

    var vm = try Lua.init(std.heap.page_allocator);
    defer vm.deinit();
    vm.openLibs();
    req.install(vm, &resolver); // <- the whole idiomatic surface

    try vm.loadString("=main",
        \\local math = require("./math")
        \\local greeting = require("./greeting")
        \\return greeting .. " — 40 + 2 = " .. math.add(40, 2)
    );
    try vm.pcall(0, 1, 0);
    std.debug.print("{s}\n", .{vm.toString(-1).?});
}
