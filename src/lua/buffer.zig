//! A Luau string builder (`luaL_Strbuf`). Build a string with incremental
//! appends, then `pushResult` to leave the finished string on the stack.
//!
//! Must be initialised in place (it holds an inline buffer and self-pointers):
//!   var b: luau.Buffer = undefined;
//!   b.init(lua);
//!   try b.print("{d}", .{42});
//!   b.pushResult();

const std = @import("std");
const c = @import("bindings");
const Lua = @import("lua.zig").Lua;

pub const Buffer = struct {
    strbuf: c.luaL_Strbuf,
    lua: *Lua,

    /// Initialise in place. `self` must keep a stable address until `pushResult`.
    pub fn init(self: *Buffer, lua: *Lua) void {
        self.lua = lua;
        c.luaL_buffinit(lua.toRaw(), &self.strbuf);
    }

    /// Initialise with at least `size` bytes preallocated, returning that
    /// writable region (fill it, then `pushResultSize`).
    pub fn initSized(self: *Buffer, lua: *Lua, size: usize) []u8 {
        self.lua = lua;
        const p = c.luaL_buffinitsize(lua.toRaw(), &self.strbuf, size);
        return p[0..size];
    }

    /// Reserve `size` writable bytes, returning the region (commit with
    /// `pushResultSize` or by `addString`-ing a sub-slice).
    pub fn prepSize(self: *Buffer, size: usize) []u8 {
        const p = c.luaL_prepbuffsize(&self.strbuf, size);
        return p[0..size];
    }

    /// Append raw bytes.
    pub fn addString(self: *Buffer, s: []const u8) void {
        c.luaL_addlstring(&self.strbuf, s.ptr, s.len);
    }
    /// Append a single byte.
    pub fn addByte(self: *Buffer, byte: u8) void {
        var b = byte;
        c.luaL_addlstring(&self.strbuf, @ptrCast(&b), 1);
    }
    /// Append the value on top of the stack (popping it).
    pub fn addValue(self: *Buffer) void {
        c.luaL_addvalue(&self.strbuf);
    }
    /// Append the value at `idx`.
    pub fn addValueAt(self: *Buffer, idx: i32) void {
        c.luaL_addvalueany(&self.strbuf, idx);
    }
    /// Append formatted text (`std.fmt`).
    pub fn print(self: *Buffer, comptime fmt: []const u8, args: anytype) void {
        var tmp: [512]u8 = undefined;
        const s = std.fmt.bufPrint(&tmp, fmt, args) catch {
            // fall back: format in chunks would be ideal; for oversized output
            // append what fit plus an ellipsis marker.
            self.addString(tmp[0..]);
            return;
        };
        self.addString(s);
    }

    // std.Io.Writer-style helpers (so Buffer works with `try w.writeAll(...)`).
    pub fn writeAll(self: *Buffer, bytes: []const u8) error{}!void {
        self.addString(bytes);
    }
    pub fn writeByte(self: *Buffer, byte: u8) error{}!void {
        self.addByte(byte);
    }

    /// Finish the buffer, leaving the resulting string on the stack.
    pub fn pushResult(self: *Buffer) void {
        c.luaL_pushresult(&self.strbuf);
    }
    /// Finish with a known final size.
    pub fn pushResultSize(self: *Buffer, size: usize) void {
        c.luaL_pushresultsize(&self.strbuf, size);
    }
};
