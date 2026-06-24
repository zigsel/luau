//! Idiomatic wrapper over Luau's CST (Concrete Syntax Tree) capture, the
//! lossless parse view a FORMATTER or codemod needs (via the C++ shim).
//!
//! Parsing here enables `storeCstData` + `captureComments`, so the result
//! retains comment trivia and, for every AST node, the exact token positions
//! and separators the lossy AST drops (commas, `=`, keyword positions, quote
//! styles). Inspect comments via `comments`/`hotcomments`, and per-node CST
//! presence + trivia via the node accessors.

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

inline fn pos(p: c.LuauPosition) Position {
    return .{ .line = p.line, .column = p.column };
}

/// The CstNode subclass attached to an AST node (mirrors LuauCstKind).
pub const CstKind = enum(c_int) {
    none = c.LUAU_CST_NONE,
    attr = c.LUAU_CST_ATTR,
    parametrized_attr = c.LUAU_CST_PARAMETRIZED_ATTR,
    expr_group = c.LUAU_CST_EXPR_GROUP,
    expr_constant_number = c.LUAU_CST_EXPR_CONSTANT_NUMBER,
    expr_constant_integer = c.LUAU_CST_EXPR_CONSTANT_INTEGER,
    expr_constant_string = c.LUAU_CST_EXPR_CONSTANT_STRING,
    expr_call = c.LUAU_CST_EXPR_CALL,
    expr_index_expr = c.LUAU_CST_EXPR_INDEX_EXPR,
    expr_function = c.LUAU_CST_EXPR_FUNCTION,
    expr_table = c.LUAU_CST_EXPR_TABLE,
    expr_op = c.LUAU_CST_EXPR_OP,
    expr_type_assertion = c.LUAU_CST_EXPR_TYPE_ASSERTION,
    expr_if_else = c.LUAU_CST_EXPR_IF_ELSE,
    expr_interp_string = c.LUAU_CST_EXPR_INTERP_STRING,
    stat_do = c.LUAU_CST_STAT_DO,
    stat_repeat = c.LUAU_CST_STAT_REPEAT,
    stat_return = c.LUAU_CST_STAT_RETURN,
    stat_local = c.LUAU_CST_STAT_LOCAL,
    stat_for = c.LUAU_CST_STAT_FOR,
    stat_for_in = c.LUAU_CST_STAT_FOR_IN,
    stat_assign = c.LUAU_CST_STAT_ASSIGN,
    stat_compound_assign = c.LUAU_CST_STAT_COMPOUND_ASSIGN,
    stat_function = c.LUAU_CST_STAT_FUNCTION,
    stat_local_function = c.LUAU_CST_STAT_LOCAL_FUNCTION,
    stat_type_alias = c.LUAU_CST_STAT_TYPE_ALIAS,
    stat_type_function = c.LUAU_CST_STAT_TYPE_FUNCTION,
    generic_type = c.LUAU_CST_GENERIC_TYPE,
    generic_type_pack = c.LUAU_CST_GENERIC_TYPE_PACK,
    type_reference = c.LUAU_CST_TYPE_REFERENCE,
    type_table = c.LUAU_CST_TYPE_TABLE,
    type_function = c.LUAU_CST_TYPE_FUNCTION,
    type_typeof = c.LUAU_CST_TYPE_TYPEOF,
    type_union = c.LUAU_CST_TYPE_UNION,
    type_intersection = c.LUAU_CST_TYPE_INTERSECTION,
    type_singleton_string = c.LUAU_CST_TYPE_SINGLETON_STRING,
    type_group = c.LUAU_CST_TYPE_GROUP,
    type_pack_explicit = c.LUAU_CST_TYPE_PACK_EXPLICIT,
    type_pack_generic = c.LUAU_CST_TYPE_PACK_GENERIC,
    /// A CstNode whose trivia the shim does not yet decode.
    other = c.LUAU_CST_OTHER,
    _,
};

/// Kind of a comment, for verbatim re-emission.
pub const CommentKind = enum(c_int) {
    line = c.LUAU_CST_COMMENT_LINE,
    block = c.LUAU_CST_COMMENT_BLOCK,
    broken = c.LUAU_CST_COMMENT_BROKEN,
    _,
};

/// A comment retained from the source, with its span and verbatim text.
pub const Comment = struct {
    kind: CommentKind,
    begin: Position,
    end: Position,
    text: []const u8,
};

/// A parse error.
pub const ParseError = struct {
    message: []const u8,
    position: Position,
};

/// One trivia entry on a node: a named token position, or an integer "info"
/// value (quote style / block depth / separator kind) whose `position` is
/// degenerate and `missing` is true.
pub const Trivia = struct {
    name: []const u8,
    position: Position,
    /// True for `Position::missing()` tokens and for integer "info" entries.
    missing: bool,
    /// Meaningful for integer "info" entries; 0 otherwise.
    value: i64,
};

/// A CST parse result. Owns the underlying arena/AST/CST; call `deinit`.
/// All returned slices/strings borrow from this handle and are valid until it.
pub const Cst = struct {
    handle: *c.LuauCst,

    /// Parse `src` with CST capture enabled. Always succeeds at the handle
    /// level; inspect `errors` for syntax errors.
    pub fn parse(src: []const u8) Cst {
        return .{ .handle = c.luau_cst_parse(src.ptr, src.len).? };
    }

    pub fn deinit(self: Cst) void {
        c.luau_cst_free(self.handle);
    }

    pub fn hasRoot(self: Cst) bool {
        return c.luau_cst_has_root(self.handle) != 0;
    }
    pub fn lineCount(self: Cst) usize {
        return c.luau_cst_line_count(self.handle);
    }

    // --- errors ------------------------------------------------------------

    pub fn errorCount(self: Cst) usize {
        return @intCast(c.luau_cst_error_count(self.handle));
    }
    pub fn errorAt(self: Cst, i: usize) ParseError {
        const ii: c_int = @intCast(i);
        return .{
            .message = std.mem.span(c.luau_cst_error_message(self.handle, ii)),
            .position = pos(c.luau_cst_error_position(self.handle, ii)),
        };
    }

    // --- comments ----------------------------------------------------------

    pub fn commentCount(self: Cst) usize {
        return @intCast(c.luau_cst_comment_count(self.handle));
    }
    pub fn commentAt(self: Cst, i: usize) Comment {
        const ii: c_int = @intCast(i);
        return .{
            .kind = @enumFromInt(c.luau_cst_comment_kind(self.handle, ii)),
            .begin = pos(c.luau_cst_comment_begin(self.handle, ii)),
            .end = pos(c.luau_cst_comment_end(self.handle, ii)),
            .text = std.mem.span(c.luau_cst_comment_text(self.handle, ii)),
        };
    }

    pub fn hotcommentCount(self: Cst) usize {
        return @intCast(c.luau_cst_hotcomment_count(self.handle));
    }
    pub fn hotcommentAt(self: Cst, i: usize) []const u8 {
        return std.mem.span(c.luau_cst_hotcomment_content(self.handle, @intCast(i)));
    }

    // --- flattened node view ----------------------------------------------

    pub fn nodeCount(self: Cst) usize {
        return @intCast(c.luau_cst_node_count(self.handle));
    }
    /// The AST kind (a `LUAU_AST_*` value; shared with the AST shim).
    pub fn nodeKind(self: Cst, i: usize) c_int {
        return c.luau_cst_node_kind(self.handle, @intCast(i));
    }
    /// Parent node index, or null for the root.
    pub fn nodeParent(self: Cst, i: usize) ?usize {
        const p = c.luau_cst_node_parent(self.handle, @intCast(i));
        return if (p < 0) null else @intCast(p);
    }
    pub fn nodeBegin(self: Cst, i: usize) Position {
        return pos(c.luau_cst_node_begin(self.handle, @intCast(i)));
    }
    pub fn nodeEnd(self: Cst, i: usize) Position {
        return pos(c.luau_cst_node_end(self.handle, @intCast(i)));
    }
    /// Whether this AST node has an associated, decoded CstNode.
    pub fn nodeHasCst(self: Cst, i: usize) bool {
        return c.luau_cst_node_has_cst(self.handle, @intCast(i)) != 0;
    }
    pub fn nodeCstKind(self: Cst, i: usize) CstKind {
        return @enumFromInt(c.luau_cst_node_cst_kind(self.handle, @intCast(i)));
    }

    // --- per-node trivia ---------------------------------------------------

    pub fn nodeTriviaCount(self: Cst, i: usize) usize {
        return @intCast(c.luau_cst_node_trivia_count(self.handle, @intCast(i)));
    }
    pub fn nodeTriviaAt(self: Cst, i: usize, j: usize) Trivia {
        const ii: c_int = @intCast(i);
        const jj: c_int = @intCast(j);
        return .{
            .name = std.mem.span(c.luau_cst_node_trivia_name(self.handle, ii, jj)),
            .position = pos(c.luau_cst_node_trivia_position(self.handle, ii, jj)),
            .missing = c.luau_cst_node_trivia_missing(self.handle, ii, jj) != 0,
            .value = c.luau_cst_node_trivia_value(self.handle, ii, jj),
        };
    }
};
