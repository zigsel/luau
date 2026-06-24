//! Idiomatic wrapper over the host-free require-path suggestion data carriers.
//!
//! The full require-suggestion engine (RequireSuggester / RequireNode /
//! FileResolver.getRequireSuggestions) is abstract and needs a live host
//! resolver that walks a project tree, so it is deliberately not bound. What is
//! host-free are the plain data structs the suggestion API yields to an editor:
//! a `Suggestion` (one require-path autocomplete target) and an `Alias` (a
//! `.luaurc`-style alias). This module lets a caller build, carry, and inspect
//! those structs without any engine state.

const std = @import("std");
const c = @import("bindings");

/// A single require-path autocomplete target: a user-facing label, the full
/// require path that would be inserted, and any attached tags. Owns its
/// storage; call `deinit`. Borrowed strings are valid until `deinit`.
pub const Suggestion = struct {
    handle: *c.LuauRequireSuggestion,

    /// Build a suggestion from `label` and the `full_path` to insert.
    pub fn init(label_text: []const u8, full_path: []const u8) Suggestion {
        return .{ .handle = c.luau_analysis_requiresuggestion_new(
            label_text.ptr,
            label_text.len,
            full_path.ptr,
            full_path.len,
        ).? };
    }

    pub fn deinit(self: Suggestion) void {
        c.luau_analysis_requiresuggestion_free(self.handle);
    }

    /// Attach a tag.
    pub fn addTag(self: Suggestion, t: []const u8) void {
        c.luau_analysis_requiresuggestion_add_tag(self.handle, t.ptr, t.len);
    }

    /// The user-facing label (borrows this suggestion's storage).
    pub fn label(self: Suggestion) []const u8 {
        return std.mem.span(c.luau_analysis_requiresuggestion_label(self.handle));
    }
    /// The full require path that would be inserted (borrowed).
    pub fn fullPath(self: Suggestion) []const u8 {
        return std.mem.span(c.luau_analysis_requiresuggestion_full_path(self.handle));
    }
    /// Number of tags attached.
    pub fn tagCount(self: Suggestion) usize {
        return @intCast(c.luau_analysis_requiresuggestion_tag_count(self.handle));
    }
    /// Tag `i` (borrowed), or "" if out of range.
    pub fn tag(self: Suggestion, i: usize) []const u8 {
        return std.mem.span(c.luau_analysis_requiresuggestion_tag(self.handle, @intCast(i)));
    }
    pub fn tags(self: Suggestion) TagIterator {
        return .{ .ptr = TagIterator.suggestionTag, .ctx = self.handle, .n = self.tagCount() };
    }
};

/// A `.luaurc`-style alias (unprefixed name, no leading `@`) plus tags. Owns its
/// storage; call `deinit`.
pub const Alias = struct {
    handle: *c.LuauRequireAlias,

    /// Build an alias from its unprefixed `name` (no leading `@`).
    pub fn init(name_text: []const u8) Alias {
        return .{ .handle = c.luau_analysis_requirealias_new(name_text.ptr, name_text.len).? };
    }

    pub fn deinit(self: Alias) void {
        c.luau_analysis_requirealias_free(self.handle);
    }

    /// Attach a tag.
    pub fn addTag(self: Alias, t: []const u8) void {
        c.luau_analysis_requirealias_add_tag(self.handle, t.ptr, t.len);
    }

    /// The unprefixed alias name (borrowed).
    pub fn name(self: Alias) []const u8 {
        return std.mem.span(c.luau_analysis_requirealias_name(self.handle));
    }
    /// Number of tags attached.
    pub fn tagCount(self: Alias) usize {
        return @intCast(c.luau_analysis_requirealias_tag_count(self.handle));
    }
    /// Tag `i` (borrowed), or "" if out of range.
    pub fn tag(self: Alias, i: usize) []const u8 {
        return std.mem.span(c.luau_analysis_requirealias_tag(self.handle, @intCast(i)));
    }
    pub fn tags(self: Alias) TagIterator {
        return .{ .ptr = TagIterator.aliasTag, .ctx = self.handle, .n = self.tagCount() };
    }
};

/// Iterates the tags of a `Suggestion` or `Alias`.
pub const TagIterator = struct {
    ptr: *const fn (*anyopaque, c_int) [*c]const u8,
    ctx: *anyopaque,
    i: usize = 0,
    n: usize,

    fn suggestionTag(ctx: *anyopaque, i: c_int) [*c]const u8 {
        const h: *c.LuauRequireSuggestion = @ptrCast(@alignCast(ctx));
        return c.luau_analysis_requiresuggestion_tag(h, i);
    }
    fn aliasTag(ctx: *anyopaque, i: c_int) [*c]const u8 {
        const h: *c.LuauRequireAlias = @ptrCast(@alignCast(ctx));
        return c.luau_analysis_requirealias_tag(h, i);
    }

    pub fn next(self: *TagIterator) ?[]const u8 {
        if (self.i >= self.n) return null;
        defer self.i += 1;
        return std.mem.span(self.ptr(self.ctx, @intCast(self.i)));
    }
};
