//! An ergonomic handle to a Luau table, pinned in the registry.

const std = @import("std");
const Lua = @import("lua.zig").Lua;
const value = @import("value.zig");

/// A pinned table you can `get`/`set`/iterate without manual stack juggling.
pub const Table = struct {
    lua: *Lua,
    id: i32,

    /// Create a fresh empty table handle.
    pub fn init(lua: *Lua) Table {
        lua.newTable();
        defer lua.pop(1);
        return .{ .lua = lua, .id = lua.ref(-1) };
    }

    /// Pin the table at `idx` (not popped). Asserts it is a table.
    pub fn fromStack(lua: *Lua, idx: i32) Table {
        std.debug.assert(lua.isTable(idx));
        lua.pushValue(idx);
        defer lua.pop(1);
        return .{ .lua = lua, .id = lua.ref(-1) };
    }

    pub fn deinit(self: Table) void {
        self.lua.unref(self.id);
    }

    /// Push this table onto the stack.
    pub fn push(self: Table) void {
        _ = self.lua.getRef(self.id);
    }

    /// `t[key]` marshalled as `V`.
    pub fn get(self: Table, comptime V: type, key: [:0]const u8) value.Error!V {
        self.push();
        defer self.lua.pop(1);
        _ = self.lua.getField(-1, key);
        defer self.lua.pop(1);
        return value.pull(V, self.lua, -1);
    }

    /// `t[key]` as `V`, or `default` if the key is missing or the wrong type.
    pub fn getOr(self: Table, comptime V: type, key: [:0]const u8, default: V) V {
        return self.get(V, key) catch default;
    }

    /// `t[key] = val` for any marshalled Zig value.
    pub fn set(self: Table, key: [:0]const u8, val: anytype) void {
        self.push();
        defer self.lua.pop(1);
        value.push(self.lua, val);
        self.lua.setField(-2, key); // table is below the just-pushed value
    }

    /// Nested read: `t[a][b]…` as `V`. `path` is a list of string keys.
    /// `t.getPath(f64, &.{ "player", "pos", "x" })`.
    pub fn getPath(self: Table, comptime V: type, path: []const [:0]const u8) value.Error!V {
        const lua = self.lua;
        self.push(); // container at -1; each step replaces it with the child
        for (path) |key| {
            if (!lua.isTable(-1)) {
                lua.pop(1);
                return error.LuaTypeMismatch;
            }
            _ = lua.getField(-1, key);
            lua.remove(-2); // drop the parent, keep the child/value
        }
        defer lua.pop(1); // the final value
        return value.pull(V, lua, -1);
    }

    /// Nested write: `t[a][b]… = val`, creating intermediate tables as needed.
    pub fn setPath(self: Table, path: []const [:0]const u8, val: anytype) void {
        std.debug.assert(path.len > 0);
        const lua = self.lua;
        self.push(); // current container at -1
        defer lua.pop(1);
        for (path[0 .. path.len - 1]) |key| {
            const t = lua.getField(-1, key);
            if (t != .table) {
                lua.pop(1); // drop the non-table
                lua.newTable();
                lua.pushValue(-1); // keep a copy to assign back
                lua.setField(-3, key); // parent[key] = newtable
            }
            lua.remove(-2); // descend: drop parent, keep child
        }
        value.push(lua, val);
        lua.setField(-2, path[path.len - 1]);
    }

    /// `#t` (array length).
    pub fn len(self: Table) usize {
        self.push();
        defer self.lua.pop(1);
        return self.lua.objLen(-1);
    }

    /// A key/value iterator. Drive it inside a stack-stable region; each
    /// `next()` leaves nothing on the stack between calls.
    pub fn iterator(self: Table) Iterator {
        return .{ .table = self };
    }

    pub const Iterator = struct {
        table: Table,
        started: bool = false,

        /// Advance; returns false when exhausted. While it returns true, the
        /// current key is at index -2 and value at -1 on the stack (caller pops
        /// them via `pair`/manual handling). Convenience: use `next` together
        /// with `keyAs`/`valueAs`.
        pub fn next(it: *Iterator) bool {
            const lua = it.table.lua;
            if (!it.started) {
                it.table.push(); // table at -1
                lua.pushNil(); // first key
                it.started = true;
            }
            // stack: ... table key
            if (lua.next(-2)) {
                return true; // stack: ... table key value
            }
            lua.pop(1); // pop table; iteration done
            return false;
        }

        /// The current key marshalled as `K` (valid while iterating).
        pub fn keyAs(it: *Iterator, comptime K: type) value.Error!K {
            return value.pull(K, it.table.lua, -2);
        }
        /// The current value marshalled as `V` (valid while iterating).
        pub fn valueAs(it: *Iterator, comptime V: type) value.Error!V {
            return value.pull(V, it.table.lua, -1);
        }
        /// Pop the current value, keeping the key for the next `next()` call.
        pub fn step(it: *Iterator) void {
            it.table.lua.pop(1);
        }
    };
};
