//! Idiomatic wrapper over richer Luau autocomplete (via the C++ shim).
//!
//! Unlike `analysis.autocomplete`, each entry carries its inferred type string,
//! documentation symbol, insert text and deprecation flag — the metadata an
//! editor needs for hover and LSP completion items.

const std = @import("std");
const c = @import("bindings");

/// The kind of an autocomplete entry (`AutocompleteEntryKind`).
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

/// A single autocomplete suggestion. All strings borrow the result's storage.
pub const Entry = struct {
    name: []const u8,
    kind: EntryKind,
    /// Stringified inferred type, or "" when the entry has no type (e.g. keywords).
    type: []const u8,
    /// Documentation symbol for hover lookup, or "" when absent.
    documentation: []const u8,
    /// Editor insert text (may differ from `name`), or "" when absent.
    insertText: []const u8,
    deprecated: bool,
};

/// Richer autocomplete suggestions at a source position. Owns storage; call `deinit`.
pub const Autocomplete = struct {
    handle: *c.LuauComplete,

    pub fn deinit(self: Autocomplete) void {
        c.luau_complete_free(self.handle);
    }

    pub fn count(self: Autocomplete) usize {
        return @intCast(c.luau_complete_count(self.handle));
    }

    pub fn get(self: Autocomplete, i: usize) Entry {
        const idx: c_int = @intCast(i);
        return .{
            .name = std.mem.span(c.luau_complete_name(self.handle, idx)),
            .kind = @enumFromInt(c.luau_complete_kind(self.handle, idx)),
            .type = std.mem.span(c.luau_complete_type_string(self.handle, idx)),
            .documentation = std.mem.span(c.luau_complete_documentation_symbol(self.handle, idx)),
            .insertText = std.mem.span(c.luau_complete_insert_text(self.handle, idx)),
            .deprecated = c.luau_complete_deprecated(self.handle, idx) != 0,
        };
    }

    pub fn iterator(self: Autocomplete) Iterator {
        return .{ .ac = self, .n = self.count() };
    }

    /// Whether an entry with the given name is suggested.
    pub fn contains(self: Autocomplete, name: []const u8) bool {
        var it = self.iterator();
        while (it.next()) |e| if (std.mem.eql(u8, e.name, name)) return true;
        return false;
    }

    pub const Iterator = struct {
        ac: Autocomplete,
        i: usize = 0,
        n: usize,
        pub fn next(it: *Iterator) ?Entry {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.ac.get(it.i);
        }
    };
};

/// Compute rich autocomplete suggestions at (0-based) `line`/`column` in `source`.
pub fn autocomplete(source: []const u8, line: u32, column: u32) Autocomplete {
    return .{ .handle = c.luau_complete(source.ptr, source.len, line, column).? };
}
