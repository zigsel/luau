//! Idiomatic wrapper over function attributes (AstAttr) and the confusables
//! lookup (via the C++ shim).

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

/// A function attribute kind (mirrors `Luau::AstAttr::Type`).
pub const AttrType = enum(c_int) {
    checked = c.LUAU_ATTR_CHECKED,
    native = c.LUAU_ATTR_NATIVE,
    deprecated = c.LUAU_ATTR_DEPRECATED,
    debug_noinline = c.LUAU_ATTR_DEBUG_NOINLINE,
    unknown = c.LUAU_ATTR_UNKNOWN,
};

/// One attribute occurrence: its kind, the id of the function it decorates
/// (group by this), and its source position.
pub const Attribute = struct {
    type: AttrType,
    function: u32,
    position: Position,
};

/// The attributes collected from a parsed source. Owns the underlying handle;
/// call `deinit` when done.
pub const Attributes = struct {
    handle: *c.LuauAttributes,

    pub fn deinit(self: Attributes) void {
        c.luau_attributes_free(self.handle);
    }

    /// Whether the source parsed without syntax errors.
    pub fn parsedOk(self: Attributes) bool {
        return c.luau_attributes_parsed_ok(self.handle) != 0;
    }

    /// Number of attribute occurrences across all functions.
    pub fn count(self: Attributes) usize {
        return @intCast(c.luau_attributes_count(self.handle));
    }

    /// The i-th attribute, or null if out of range.
    pub fn at(self: Attributes, i: usize) ?Attribute {
        const raw = c.luau_attributes_type(self.handle, @intCast(i));
        if (raw < 0) return null;
        const fid = c.luau_attributes_function(self.handle, @intCast(i));
        const pos = c.luau_attributes_position(self.handle, @intCast(i));
        return .{
            .type = @enumFromInt(raw),
            .function = @intCast(fid),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }
};

/// Parse `src` and collect the attributes of every function. The returned
/// `Attributes` must be `deinit`ed.
pub fn parse(src: []const u8) error{OutOfMemory}!Attributes {
    const handle = c.luau_attributes_parse(src.ptr, src.len) orelse return error.OutOfMemory;
    return .{ .handle = handle };
}

/// If `codepoint` is a known visually-confusable character, the suggested
/// replacement; otherwise null.
pub fn confusableSuggestion(codepoint: u32) ?[]const u8 {
    const s = c.luau_confusable_suggestion(codepoint) orelse return null;
    return std.mem.span(s);
}

/// Whether `codepoint` is a known confusable.
pub fn isConfusable(codepoint: u32) bool {
    return c.luau_confusable_is(codepoint) != 0;
}
