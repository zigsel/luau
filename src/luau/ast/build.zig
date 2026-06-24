//! Idiomatic wrapper over Luau AST construction + compilation (the transpiler
//! path, via the C++ shim). Build an AST out of `Node` handles, then `compile`
//! it straight to bytecode.

const std = @import("std");
const c = @import("bindings");

/// Binary operators (mirror Luau::AstExprBinary::Op).
pub const BinaryOp = enum(c_int) {
    add = c.LUAU_AST_BINOP_ADD,
    sub = c.LUAU_AST_BINOP_SUB,
    mul = c.LUAU_AST_BINOP_MUL,
    div = c.LUAU_AST_BINOP_DIV,
    floordiv = c.LUAU_AST_BINOP_FLOORDIV,
    mod = c.LUAU_AST_BINOP_MOD,
    pow = c.LUAU_AST_BINOP_POW,
    concat = c.LUAU_AST_BINOP_CONCAT,
    compare_ne = c.LUAU_AST_BINOP_COMPARE_NE,
    compare_eq = c.LUAU_AST_BINOP_COMPARE_EQ,
    compare_lt = c.LUAU_AST_BINOP_COMPARE_LT,
    compare_le = c.LUAU_AST_BINOP_COMPARE_LE,
    compare_gt = c.LUAU_AST_BINOP_COMPARE_GT,
    compare_ge = c.LUAU_AST_BINOP_COMPARE_GE,
    @"and" = c.LUAU_AST_BINOP_AND,
    @"or" = c.LUAU_AST_BINOP_OR,
};

/// Unary operators (mirror Luau::AstExprUnary::Op).
pub const UnaryOp = enum(c_int) {
    not = c.LUAU_AST_UNOP_NOT,
    minus = c.LUAU_AST_UNOP_MINUS,
    len = c.LUAU_AST_UNOP_LEN,
};

/// Compound-assignment operators (mirror Luau::AstExprBinary::Op, reused by
/// AstStatCompoundAssign — comparison ops are not valid here).
pub const CompoundOp = enum(c_int) {
    add = c.LUAU_AST_BINOP_ADD,
    sub = c.LUAU_AST_BINOP_SUB,
    mul = c.LUAU_AST_BINOP_MUL,
    div = c.LUAU_AST_BINOP_DIV,
    floordiv = c.LUAU_AST_BINOP_FLOORDIV,
    mod = c.LUAU_AST_BINOP_MOD,
    pow = c.LUAU_AST_BINOP_POW,
    concat = c.LUAU_AST_BINOP_CONCAT,
};

/// Table item kinds (mirror Luau::AstExprTable::Item::Kind).
pub const TableItemKind = enum(c_int) {
    list = c.LUAU_AST_TABLEITEM_LIST,
    record = c.LUAU_AST_TABLEITEM_RECORD,
    general = c.LUAU_AST_TABLEITEM_GENERAL,
};

/// An opaque AST node living in the builder's arena (valid until `deinit`).
pub const Node = *c.LuauAstNode;

/// An opaque AstLocal declaration (also arena-owned). Distinct conceptually
/// from `Node`, though it shares the same opaque-pointer representation.
pub const Local = *c.LuauAstNode;

/// A single table constructor entry.
pub const TableItem = struct {
    kind: TableItemKind,
    key: ?Node = null,
    value: Node,
};

/// Reinterpret a `[]const Node` as the `[*c]?*LuauAstNode` the C shim expects.
/// `Node` (`*T`) and `?*T` share layout, so this is a no-op pointer cast.
inline fn nodePtr(nodes: []const Node) [*c]?*c.LuauAstNode {
    return @constCast(@ptrCast(nodes.ptr));
}

/// Owns an arena allocator + name table; every node it produces lives in that
/// arena. Call `deinit` to free all nodes at once.
pub const Builder = struct {
    handle: *c.LuauAstBuilder,

    pub fn init() Builder {
        return .{ .handle = c.luau_astbuild_new().? };
    }
    pub fn deinit(self: Builder) void {
        c.luau_astbuild_free(self.handle);
    }

    // --- expressions -------------------------------------------------------

    pub fn nil(self: Builder) Node {
        return c.luau_astbuild_constant_nil(self.handle).?;
    }
    pub fn boolean(self: Builder, value: bool) Node {
        return c.luau_astbuild_constant_bool(self.handle, @intFromBool(value)).?;
    }
    pub fn number(self: Builder, value: f64) Node {
        return c.luau_astbuild_constant_number(self.handle, value).?;
    }
    pub fn string(self: Builder, value: []const u8) Node {
        return c.luau_astbuild_constant_string(self.handle, value.ptr, value.len).?;
    }
    pub fn global(self: Builder, name: [:0]const u8) Node {
        return c.luau_astbuild_global(self.handle, name.ptr).?;
    }
    pub fn binary(self: Builder, op: BinaryOp, lhs: Node, rhs: Node) Node {
        return c.luau_astbuild_binary(self.handle, @intFromEnum(op), lhs, rhs).?;
    }
    pub fn unary(self: Builder, op: UnaryOp, e: Node) Node {
        return c.luau_astbuild_unary(self.handle, @intFromEnum(op), e).?;
    }
    pub fn call(self: Builder, func: Node, args: []const Node) Node {
        return c.luau_astbuild_call(self.handle, func, nodePtr(args), @intCast(args.len)).?;
    }
    pub fn indexName(self: Builder, e: Node, name: [:0]const u8) Node {
        return c.luau_astbuild_index_name(self.handle, e, name.ptr).?;
    }
    pub fn group(self: Builder, e: Node) Node {
        return c.luau_astbuild_group(self.handle, e).?;
    }
    pub fn integer(self: Builder, value: i64) Node {
        return c.luau_astbuild_constant_integer(self.handle, value).?;
    }
    pub fn exprLocal(self: Builder, l: Local) Node {
        return c.luau_astbuild_expr_local(self.handle, l).?;
    }
    pub fn varargs(self: Builder) Node {
        return c.luau_astbuild_varargs(self.handle).?;
    }
    pub fn indexExpr(self: Builder, e: Node, index: Node) Node {
        return c.luau_astbuild_index_expr(self.handle, e, index).?;
    }
    /// An anonymous function. `args` are locals; `body` are statements (wrapped
    /// into a block internally). Set `vararg` for a `...` parameter.
    pub fn function(self: Builder, args: []const Local, vararg: bool, body: []const Node) Node {
        return c.luau_astbuild_function(
            self.handle,
            nodePtr(args),
            @intCast(args.len),
            @intFromBool(vararg),
            nodePtr(body),
            @intCast(body.len),
        ).?;
    }
    pub fn table(self: Builder, gpa: std.mem.Allocator, items: []const TableItem) !Node {
        const kinds = try gpa.alloc(c_int, items.len);
        defer gpa.free(kinds);
        const keys = try gpa.alloc(?*c.LuauAstNode, items.len);
        defer gpa.free(keys);
        const values = try gpa.alloc(?*c.LuauAstNode, items.len);
        defer gpa.free(values);
        for (items, 0..) |it, i| {
            kinds[i] = @intFromEnum(it.kind);
            keys[i] = it.key;
            values[i] = it.value;
        }
        return c.luau_astbuild_table(self.handle, kinds.ptr, keys.ptr, values.ptr, @intCast(items.len)).?;
    }
    pub fn typeAssertion(self: Builder, e: Node, annotation: Node) Node {
        return c.luau_astbuild_type_assertion(self.handle, e, annotation).?;
    }
    pub fn ifElse(self: Builder, cond: Node, true_expr: Node, false_expr: Node) Node {
        return c.luau_astbuild_if_else(self.handle, cond, true_expr, false_expr).?;
    }
    /// An interpolated string. `strings` holds the raw chunks; `exprs` the
    /// embedded expressions. `strings.len` must equal `exprs.len + 1`.
    pub fn interpString(self: Builder, gpa: std.mem.Allocator, strings: []const []const u8, exprs: []const Node) !Node {
        const ptrs = try gpa.alloc([*c]const u8, strings.len);
        defer gpa.free(ptrs);
        const lens = try gpa.alloc(usize, strings.len);
        defer gpa.free(lens);
        for (strings, 0..) |s, i| {
            ptrs[i] = s.ptr;
            lens[i] = s.len;
        }
        return c.luau_astbuild_interp_string(
            self.handle,
            ptrs.ptr,
            lens.ptr,
            @intCast(strings.len),
            nodePtr(exprs),
            @intCast(exprs.len),
        ).?;
    }

    // --- locals & types ----------------------------------------------------

    /// Declare a local variable. `annotation` is an optional AstType node.
    pub fn declareLocal(self: Builder, name: [:0]const u8, annotation: ?Node) Local {
        return c.luau_astbuild_local(self.handle, name.ptr, annotation orelse null).?;
    }
    /// A type reference by name (e.g. `number`, `MyType`).
    pub fn typeReference(self: Builder, name: [:0]const u8) Node {
        return c.luau_astbuild_type_reference(self.handle, name.ptr).?;
    }

    // --- statements --------------------------------------------------------

    pub fn ret(self: Builder, exprs: []const Node) Node {
        return c.luau_astbuild_return(self.handle, nodePtr(exprs), @intCast(exprs.len)).?;
    }
    pub fn exprStat(self: Builder, e: Node) Node {
        return c.luau_astbuild_expr_stat(self.handle, e).?;
    }
    pub fn block(self: Builder, stats: []const Node) Node {
        return c.luau_astbuild_block(self.handle, nodePtr(stats), @intCast(stats.len)).?;
    }
    /// `then_block` must be a block node; `else_stat` may be null, a block, or a
    /// nested `if` node (for `elseif`).
    pub fn ifStat(self: Builder, cond: Node, then_block: Node, else_stat: ?Node) Node {
        return c.luau_astbuild_if(self.handle, cond, then_block, else_stat orelse null).?;
    }
    pub fn whileStat(self: Builder, cond: Node, body: Node) Node {
        return c.luau_astbuild_while(self.handle, cond, body).?;
    }
    pub fn repeatStat(self: Builder, cond: Node, body: Node) Node {
        return c.luau_astbuild_repeat(self.handle, cond, body).?;
    }
    pub fn breakStat(self: Builder) Node {
        return c.luau_astbuild_break(self.handle).?;
    }
    pub fn continueStat(self: Builder) Node {
        return c.luau_astbuild_continue(self.handle).?;
    }
    /// Numeric `for var = from, to[, step] do body end`. `step` is optional.
    pub fn forStat(self: Builder, variable: Local, from: Node, to: Node, step: ?Node, body: Node) Node {
        return c.luau_astbuild_for(self.handle, variable, from, to, step orelse null, body).?;
    }
    pub fn forIn(self: Builder, vars: []const Local, values: []const Node, body: Node) Node {
        return c.luau_astbuild_for_in(
            self.handle,
            nodePtr(vars),
            @intCast(vars.len),
            nodePtr(values),
            @intCast(values.len),
            body,
        ).?;
    }
    pub fn assign(self: Builder, vars: []const Node, values: []const Node) Node {
        return c.luau_astbuild_assign(
            self.handle,
            nodePtr(vars),
            @intCast(vars.len),
            nodePtr(values),
            @intCast(values.len),
        ).?;
    }
    pub fn compoundAssign(self: Builder, op: CompoundOp, variable: Node, value: Node) Node {
        return c.luau_astbuild_compound_assign(self.handle, @intFromEnum(op), variable, value).?;
    }
    /// `local vars = values`. Type annotations live on the declared locals.
    pub fn localStat(self: Builder, vars: []const Local, values: []const Node) Node {
        return c.luau_astbuild_local_stat(
            self.handle,
            nodePtr(vars),
            @intCast(vars.len),
            nodePtr(values),
            @intCast(values.len),
        ).?;
    }
    /// `function name(...) ... end`; `func` is an `function` expr node.
    pub fn functionStat(self: Builder, name: Node, func: Node) Node {
        return c.luau_astbuild_function_stat(self.handle, name, func).?;
    }
    /// `local function name(...) ... end`; `func` is an `function` expr node.
    pub fn localFunction(self: Builder, name: Local, func: Node) Node {
        return c.luau_astbuild_local_function(self.handle, name, func).?;
    }

    // --- compilation -------------------------------------------------------

    pub const CompileError = error{ Compile, OutOfMemory };

    /// Compile `rootBlock` (an AstStatBlock node) to bytecode. The returned
    /// slice is owned by `gpa`. On a compile error the message is logged and
    /// `error.Compile` is returned.
    pub fn compile(self: Builder, gpa: std.mem.Allocator, rootBlock: Node) ![]u8 {
        var len: c_int = 0;
        var err: [*c]u8 = null;
        const buf = c.luau_astbuild_compile(self.handle, rootBlock, &len, &err);
        if (buf == null) {
            if (err) |e| {
                std.log.scoped(.luau).err("ast compile: {s}", .{std.mem.span(e)});
                std.c.free(e);
            }
            return error.Compile;
        }
        defer std.c.free(buf);
        const n: usize = @intCast(len);
        const out = try gpa.alloc(u8, n);
        @memcpy(out, @as([*]u8, @ptrCast(buf))[0..n]);
        return out;
    }
};
