//! Idiomatic wrapper over Luau fragment (incremental) autocomplete.
//!
//! `fragmentAutocomplete` reuses a previously type-checked ("stale") module and
//! re-analyses only the fragment of source around the cursor, which is what an
//! editor wants for fast keystroke-by-keystroke completion. Each entry also
//! carries its documentation symbol — the lookup key into a documentation
//! database — for hover.

const std = @import("std");
const c = @import("bindings");

/// The kind of an autocomplete entry (`AutocompleteEntryKind`), mirrors
/// `analysis.autocomplete2.EntryKind`.
pub const EntryKind = enum(c_int) {
    property = 0,
    binding = 1,
    keyword = 2,
    string = 3,
    type = 4,
    module = 5,
    generated_function = 6,
    require_path = 7,
    hot_comment = 8,
    _,
};

/// A single fragment-autocomplete suggestion. All strings borrow result storage.
pub const Entry = struct {
    name: []const u8,
    kind: EntryKind,
    /// Documentation symbol for hover lookup, or "" when absent.
    documentation: []const u8,
    deprecated: bool,
};

/// Fragment autocomplete suggestions. Owns storage; call `deinit`.
pub const Fragment = struct {
    handle: *c.LuauFragment,

    pub fn deinit(self: Fragment) void {
        c.luau_fragment_free(self.handle);
    }

    /// Whether the incremental fragment type-check + completion succeeded.
    pub fn ok(self: Fragment) bool {
        return c.luau_fragment_ok(self.handle) != 0;
    }

    pub fn count(self: Fragment) usize {
        return @intCast(c.luau_fragment_count(self.handle));
    }

    pub fn get(self: Fragment, i: usize) Entry {
        const idx: c_int = @intCast(i);
        return .{
            .name = std.mem.span(c.luau_fragment_name(self.handle, idx)),
            .kind = @enumFromInt(c.luau_fragment_kind(self.handle, idx)),
            .documentation = std.mem.span(c.luau_fragment_documentation_symbol(self.handle, idx)),
            .deprecated = c.luau_fragment_deprecated(self.handle, idx) != 0,
        };
    }

    pub fn iterator(self: Fragment) Iterator {
        return .{ .frag = self, .n = self.count() };
    }

    /// Whether an entry with the given name is suggested.
    pub fn contains(self: Fragment, name: []const u8) bool {
        var it = self.iterator();
        while (it.next()) |e| if (std.mem.eql(u8, e.name, name)) return true;
        return false;
    }

    pub const Iterator = struct {
        frag: Fragment,
        i: usize = 0,
        n: usize,
        pub fn next(it: *Iterator) ?Entry {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.frag.get(it.i);
        }
    };
};

/// Compute incremental autocomplete suggestions at (0-based) `line`/`column`.
///
/// `stale_source` is the source that was last type-checked; `new_source` is the
/// current buffer contents being edited. Only the changed fragment is re-analysed.
pub fn autocomplete(stale_source: []const u8, new_source: []const u8, line: u32, column: u32) Fragment {
    return .{ .handle = c.luau_fragment_autocomplete(
        stale_source.ptr,
        stale_source.len,
        new_source.ptr,
        new_source.len,
        line,
        column,
    ).? };
}
