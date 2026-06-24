//! Idiomatic wrapper over the Luau Ast parser (via the C++ shim).

const std = @import("std");
const c = @import("bindings");

/// Construct an AST programmatically and compile it to bytecode (transpiler path).
pub const build = @import("ast/build.zig");

/// Pretty-print (transpile) Luau source into a normalized form.
pub const print = @import("ast/print.zig");

/// Tokenize Luau source into a token list.
pub const lex = @import("ast/lex.zig");

/// Lossless CST capture (comments + per-node token positions) for formatters
/// and codemods.
pub const cst = @import("ast/cst.zig");

/// Function attributes (@native/@checked/@deprecated/...) for a parsed source,
/// plus the Unicode confusables lookup.
pub const attributes = @import("ast/attributes.zig");

/// Serialize a parsed AST to JSON (for external tooling / editors).
pub const json = @import("ast/json.zig");

/// A 0-based source position.
pub const Position = struct {
    line: u32,
    column: u32,
};

/// A syntax error from a parse.
pub const ParseError = struct {
    message: []const u8,
    position: Position,
};

/// The result of parsing Luau source. Owns the underlying allocator/AST; call
/// `deinit` when done. Diagnostics borrow its storage and are valid until then.
pub const Parsed = struct {
    handle: *c.LuauParseResult,

    pub fn deinit(self: Parsed) void {
        c.luau_ast_parse_free(self.handle);
    }

    /// Whether the source parsed without any syntax errors.
    pub fn ok(self: Parsed) bool {
        return self.errorCount() == 0;
    }
    /// Whether a (possibly partial) AST root was produced.
    pub fn hasRoot(self: Parsed) bool {
        return c.luau_ast_has_root(self.handle) != 0;
    }
    /// Number of source lines.
    pub fn lineCount(self: Parsed) usize {
        return c.luau_ast_line_count(self.handle);
    }

    pub fn errorCount(self: Parsed) usize {
        return @intCast(c.luau_ast_error_count(self.handle));
    }
    /// The `i`-th syntax error (message borrows the parse's storage).
    pub fn getError(self: Parsed, i: usize) ParseError {
        const msg = c.luau_ast_error_message(self.handle, @intCast(i));
        const pos = c.luau_ast_error_position(self.handle, @intCast(i));
        return .{
            .message = std.mem.span(msg),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }

    pub fn hotcommentCount(self: Parsed) usize {
        return @intCast(c.luau_ast_hotcomment_count(self.handle));
    }
    /// The `i`-th hot comment content (e.g. `!strict`), borrowing storage.
    pub fn hotcomment(self: Parsed, i: usize) []const u8 {
        return std.mem.span(c.luau_ast_hotcomment_content(self.handle, @intCast(i)));
    }

    /// Iterate the syntax errors.
    pub fn errors(self: Parsed) ErrorIterator {
        return .{ .parsed = self, .n = self.errorCount() };
    }

    /// Number of nodes in the flattened AST.
    pub fn nodeCount(self: Parsed) usize {
        return @intCast(c.luau_ast_node_count(self.handle));
    }
    /// The `i`-th node (pre-order; index 0 is the root block when present).
    pub fn node(self: Parsed, i: usize) Node {
        return .{ .parsed = self, .index = i };
    }
    /// The root node, or null if nothing parsed.
    pub fn root(self: Parsed) ?Node {
        return if (self.nodeCount() == 0) null else self.node(0);
    }

    pub const ErrorIterator = struct {
        parsed: Parsed,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ErrorIterator) ?ParseError {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.parsed.getError(it.i);
        }
    };
};

/// The kind of an AST node.
pub const NodeKind = enum(c_int) {
    unknown = c.LUAU_AST_UNKNOWN,
    stat_block = c.LUAU_AST_STAT_BLOCK,
    stat_if = c.LUAU_AST_STAT_IF,
    stat_while = c.LUAU_AST_STAT_WHILE,
    stat_repeat = c.LUAU_AST_STAT_REPEAT,
    stat_break = c.LUAU_AST_STAT_BREAK,
    stat_continue = c.LUAU_AST_STAT_CONTINUE,
    stat_return = c.LUAU_AST_STAT_RETURN,
    stat_expr = c.LUAU_AST_STAT_EXPR,
    stat_local = c.LUAU_AST_STAT_LOCAL,
    stat_for = c.LUAU_AST_STAT_FOR,
    stat_for_in = c.LUAU_AST_STAT_FOR_IN,
    stat_assign = c.LUAU_AST_STAT_ASSIGN,
    stat_compound_assign = c.LUAU_AST_STAT_COMPOUND_ASSIGN,
    stat_function = c.LUAU_AST_STAT_FUNCTION,
    stat_local_function = c.LUAU_AST_STAT_LOCAL_FUNCTION,
    stat_type_alias = c.LUAU_AST_STAT_TYPE_ALIAS,
    stat_type_function = c.LUAU_AST_STAT_TYPE_FUNCTION,
    stat_error = c.LUAU_AST_STAT_ERROR,
    expr_group = c.LUAU_AST_EXPR_GROUP,
    expr_constant_nil = c.LUAU_AST_EXPR_CONSTANT_NIL,
    expr_constant_bool = c.LUAU_AST_EXPR_CONSTANT_BOOL,
    expr_constant_number = c.LUAU_AST_EXPR_CONSTANT_NUMBER,
    expr_constant_string = c.LUAU_AST_EXPR_CONSTANT_STRING,
    expr_constant_integer = c.LUAU_AST_EXPR_CONSTANT_INTEGER,
    expr_local = c.LUAU_AST_EXPR_LOCAL,
    expr_global = c.LUAU_AST_EXPR_GLOBAL,
    expr_varargs = c.LUAU_AST_EXPR_VARARGS,
    expr_call = c.LUAU_AST_EXPR_CALL,
    expr_index_name = c.LUAU_AST_EXPR_INDEX_NAME,
    expr_index_expr = c.LUAU_AST_EXPR_INDEX_EXPR,
    expr_function = c.LUAU_AST_EXPR_FUNCTION,
    expr_table = c.LUAU_AST_EXPR_TABLE,
    expr_unary = c.LUAU_AST_EXPR_UNARY,
    expr_binary = c.LUAU_AST_EXPR_BINARY,
    expr_type_assertion = c.LUAU_AST_EXPR_TYPE_ASSERTION,
    expr_if_else = c.LUAU_AST_EXPR_IF_ELSE,
    expr_interp_string = c.LUAU_AST_EXPR_INTERP_STRING,
    expr_error = c.LUAU_AST_EXPR_ERROR,
    type_reference = c.LUAU_AST_TYPE_REFERENCE,
    type_table = c.LUAU_AST_TYPE_TABLE,
    type_function = c.LUAU_AST_TYPE_FUNCTION,
    type_typeof = c.LUAU_AST_TYPE_TYPEOF,
    type_union = c.LUAU_AST_TYPE_UNION,
    type_intersection = c.LUAU_AST_TYPE_INTERSECTION,
    type_optional = c.LUAU_AST_TYPE_OPTIONAL,
    type_group = c.LUAU_AST_TYPE_GROUP,
    type_error = c.LUAU_AST_TYPE_ERROR,
    type_singleton_string = c.LUAU_AST_TYPE_SINGLETON_STRING,
    _,
};

/// Binary operator (payload of `expr_binary` nodes, via `Node.integer`).
pub const BinaryOp = enum(i64) {
    add, sub, mul, div, floordiv, mod, pow, concat,
    compare_ne, compare_eq, compare_lt, compare_le, compare_gt, compare_ge,
    @"and", @"or", _,
};

/// Unary operator (payload of `expr_unary` nodes, via `Node.integer`).
pub const UnaryOp = enum(i64) { not, minus, len, _ };

/// A handle to a node in a flattened AST (index into the parse's node list).
pub const Node = struct {
    parsed: Parsed,
    index: usize,

    pub fn kind(self: Node) NodeKind {
        return @enumFromInt(c.luau_ast_node_kind(self.parsed.handle, @intCast(self.index)));
    }
    /// The parent node, or null for the root.
    pub fn parent(self: Node) ?Node {
        const p = c.luau_ast_node_parent(self.parsed.handle, @intCast(self.index));
        return if (p < 0) null else .{ .parsed = self.parsed, .index = @intCast(p) };
    }
    pub fn begin(self: Node) Position {
        const p = c.luau_ast_node_begin(self.parsed.handle, @intCast(self.index));
        return .{ .line = p.line, .column = p.column };
    }
    pub fn end(self: Node) Position {
        const p = c.luau_ast_node_end(self.parsed.handle, @intCast(self.index));
        return .{ .line = p.line, .column = p.column };
    }
    /// String payload (identifier, string literal, type name), or "" if none.
    pub fn string(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_node_string(self.parsed.handle, @intCast(self.index)));
    }
    pub fn number(self: Node) f64 {
        return c.luau_ast_node_number(self.parsed.handle, @intCast(self.index));
    }
    pub fn integer(self: Node) i64 {
        return c.luau_ast_node_integer(self.parsed.handle, @intCast(self.index));
    }
    pub fn boolean(self: Node) bool {
        return c.luau_ast_node_boolean(self.parsed.handle, @intCast(self.index)) != 0;
    }
    /// The binary operator (only meaningful for `expr_binary`).
    pub fn binaryOp(self: Node) BinaryOp {
        return @enumFromInt(c.luau_ast_binary_op(self.handle(), self.ix()));
    }
    /// The unary operator (only meaningful for `expr_unary`).
    pub fn unaryOp(self: Node) UnaryOp {
        return @enumFromInt(c.luau_ast_unary_op(self.handle(), self.ix()));
    }

    /// Iterate this node's direct children (in source order).
    pub fn children(self: Node) ChildIterator {
        return .{ .parsed = self.parsed, .parent_index = self.index, .i = self.index + 1 };
    }

    // ---- typed field accessors ----------------------------------------------
    //
    // Each returns the named child node / scalar field for a specific node kind.
    // Calling an accessor on a node of the wrong kind yields null / "" / 0.

    // Wrap a flat index returned by a shim accessor: <0 becomes null.
    inline fn nodeAt(self: Node, idx: c_int) ?Node {
        return if (idx < 0) null else .{ .parsed = self.parsed, .index = @intCast(idx) };
    }
    inline fn handle(self: Node) *c.LuauParseResult {
        return self.parsed.handle;
    }
    inline fn ix(self: Node) c_int {
        return @intCast(self.index);
    }

    /// expr_binary: left operand.
    pub fn binaryLeft(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_binary_left(self.handle(), self.ix()));
    }
    /// expr_binary: right operand.
    pub fn binaryRight(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_binary_right(self.handle(), self.ix()));
    }

    /// expr_unary: operand.
    pub fn unaryOperand(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_unary_operand(self.handle(), self.ix()));
    }

    /// expr_group: inner expression.
    pub fn groupExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_group_expr(self.handle(), self.ix()));
    }

    /// expr_call: the called function/expression.
    pub fn callFunc(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_call_func(self.handle(), self.ix()));
    }
    /// expr_call: whether this is a method (`a:b()`) call.
    pub fn callSelf(self: Node) bool {
        return c.luau_ast_call_self(self.handle(), self.ix()) != 0;
    }
    /// expr_call: number of arguments.
    pub fn callArgCount(self: Node) usize {
        return @intCast(c.luau_ast_call_arg_count(self.handle(), self.ix()));
    }
    /// expr_call: the `j`-th argument.
    pub fn callArg(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_call_arg(self.handle(), self.ix(), @intCast(j)));
    }

    /// expr_index_name: indexed expression.
    pub fn indexNameExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_index_name_expr(self.handle(), self.ix()));
    }
    /// expr_index_name: the index identifier.
    pub fn indexNameIndex(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_index_name_index(self.handle(), self.ix()));
    }

    /// expr_index_expr: indexed expression.
    pub fn indexExprExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_index_expr_expr(self.handle(), self.ix()));
    }
    /// expr_index_expr: the index expression.
    pub fn indexExprIndex(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_index_expr_index(self.handle(), self.ix()));
    }

    /// expr_function: number of parameters.
    pub fn functionParamCount(self: Node) usize {
        return @intCast(c.luau_ast_function_param_count(self.handle(), self.ix()));
    }
    /// expr_function: the `j`-th parameter name.
    pub fn functionParamName(self: Node, j: usize) []const u8 {
        return std.mem.span(c.luau_ast_function_param_name(self.handle(), self.ix(), @intCast(j)));
    }
    /// expr_function: whether it accepts `...`.
    pub fn functionVararg(self: Node) bool {
        return c.luau_ast_function_vararg(self.handle(), self.ix()) != 0;
    }
    /// expr_function: the body block.
    pub fn functionBody(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_function_body(self.handle(), self.ix()));
    }

    /// expr_table: number of items.
    pub fn tableItemCount(self: Node) usize {
        return @intCast(c.luau_ast_table_item_count(self.handle(), self.ix()));
    }
    /// expr_table: the kind of the `j`-th item.
    pub fn tableItemKind(self: Node, j: usize) TableItemKind {
        return @enumFromInt(c.luau_ast_table_item_kind(self.handle(), self.ix(), @intCast(j)));
    }
    /// expr_table: the `j`-th item's key (null for list items).
    pub fn tableItemKey(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_table_item_key(self.handle(), self.ix(), @intCast(j)));
    }
    /// expr_table: the `j`-th item's value.
    pub fn tableItemValue(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_table_item_value(self.handle(), self.ix(), @intCast(j)));
    }

    /// expr_type_assertion: the asserted expression.
    pub fn typeAssertionExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_type_assertion_expr(self.handle(), self.ix()));
    }
    /// expr_type_assertion: the type annotation.
    pub fn typeAssertionAnnotation(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_type_assertion_annotation(self.handle(), self.ix()));
    }

    /// expr_if_else: condition.
    pub fn ifElseCondition(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_ifelse_condition(self.handle(), self.ix()));
    }
    /// expr_if_else: the `then` branch expression.
    pub fn ifElseTrue(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_ifelse_trueexpr(self.handle(), self.ix()));
    }
    /// expr_if_else: the `else` branch expression.
    pub fn ifElseFalse(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_ifelse_falseexpr(self.handle(), self.ix()));
    }

    /// expr_interp_string: number of interpolated expressions.
    pub fn interpExprCount(self: Node) usize {
        return @intCast(c.luau_ast_interp_expr_count(self.handle(), self.ix()));
    }
    /// expr_interp_string: the `j`-th interpolated expression.
    pub fn interpExpr(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_interp_expr(self.handle(), self.ix(), @intCast(j)));
    }

    /// stat_block: number of statements.
    pub fn blockStatCount(self: Node) usize {
        return @intCast(c.luau_ast_block_stat_count(self.handle(), self.ix()));
    }
    /// stat_block: the `j`-th statement.
    pub fn blockStat(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_block_stat(self.handle(), self.ix(), @intCast(j)));
    }

    /// stat_if: condition.
    pub fn ifCondition(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_if_condition(self.handle(), self.ix()));
    }
    /// stat_if: the `then` body block.
    pub fn ifThen(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_if_thenbody(self.handle(), self.ix()));
    }
    /// stat_if: the `else`/`elseif` body, or null.
    pub fn ifElse(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_if_elsebody(self.handle(), self.ix()));
    }

    /// stat_while: condition.
    pub fn whileCondition(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_while_condition(self.handle(), self.ix()));
    }
    /// stat_while: body block.
    pub fn whileBody(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_while_body(self.handle(), self.ix()));
    }

    /// stat_repeat: condition.
    pub fn repeatCondition(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_repeat_condition(self.handle(), self.ix()));
    }
    /// stat_repeat: body block.
    pub fn repeatBody(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_repeat_body(self.handle(), self.ix()));
    }

    /// stat_for: loop variable name.
    pub fn forVar(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_for_var(self.handle(), self.ix()));
    }
    /// stat_for: start expression.
    pub fn forFrom(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_for_from(self.handle(), self.ix()));
    }
    /// stat_for: end expression.
    pub fn forTo(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_for_to(self.handle(), self.ix()));
    }
    /// stat_for: step expression, or null.
    pub fn forStep(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_for_step(self.handle(), self.ix()));
    }
    /// stat_for: body block.
    pub fn forBody(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_for_body(self.handle(), self.ix()));
    }

    /// stat_for_in: number of loop variables.
    pub fn forInVarCount(self: Node) usize {
        return @intCast(c.luau_ast_forin_var_count(self.handle(), self.ix()));
    }
    /// stat_for_in: the `j`-th loop variable name.
    pub fn forInVar(self: Node, j: usize) []const u8 {
        return std.mem.span(c.luau_ast_forin_var(self.handle(), self.ix(), @intCast(j)));
    }
    /// stat_for_in: number of iterator value expressions.
    pub fn forInValueCount(self: Node) usize {
        return @intCast(c.luau_ast_forin_value_count(self.handle(), self.ix()));
    }
    /// stat_for_in: the `j`-th iterator value expression.
    pub fn forInValue(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_forin_value(self.handle(), self.ix(), @intCast(j)));
    }
    /// stat_for_in: body block.
    pub fn forInBody(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_forin_body(self.handle(), self.ix()));
    }

    /// stat_return: number of returned expressions.
    pub fn returnExprCount(self: Node) usize {
        return @intCast(c.luau_ast_return_expr_count(self.handle(), self.ix()));
    }
    /// stat_return: the `j`-th returned expression.
    pub fn returnExpr(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_return_expr(self.handle(), self.ix(), @intCast(j)));
    }

    /// stat_expr: the wrapped expression.
    pub fn statExprExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_stat_expr_expr(self.handle(), self.ix()));
    }

    /// stat_local: number of declared variables.
    pub fn localVarCount(self: Node) usize {
        return @intCast(c.luau_ast_local_var_count(self.handle(), self.ix()));
    }
    /// stat_local: the `j`-th declared variable name.
    pub fn localVarName(self: Node, j: usize) []const u8 {
        return std.mem.span(c.luau_ast_local_var_name(self.handle(), self.ix(), @intCast(j)));
    }
    /// stat_local: number of value expressions.
    pub fn localValueCount(self: Node) usize {
        return @intCast(c.luau_ast_local_value_count(self.handle(), self.ix()));
    }
    /// stat_local: the `j`-th value expression.
    pub fn localValue(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_local_value(self.handle(), self.ix(), @intCast(j)));
    }

    /// stat_assign: number of assignment targets.
    pub fn assignLhsCount(self: Node) usize {
        return @intCast(c.luau_ast_assign_lhs_count(self.handle(), self.ix()));
    }
    /// stat_assign: the `j`-th assignment target.
    pub fn assignLhs(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_assign_lhs(self.handle(), self.ix(), @intCast(j)));
    }
    /// stat_assign: number of assigned values.
    pub fn assignRhsCount(self: Node) usize {
        return @intCast(c.luau_ast_assign_rhs_count(self.handle(), self.ix()));
    }
    /// stat_assign: the `j`-th assigned value.
    pub fn assignRhs(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_assign_rhs(self.handle(), self.ix(), @intCast(j)));
    }

    /// stat_compound_assign: the compound operator.
    pub fn compoundOp(self: Node) BinaryOp {
        return @enumFromInt(c.luau_ast_compound_op(self.handle(), self.ix()));
    }
    /// stat_compound_assign: the assignment target.
    pub fn compoundLhs(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_compound_lhs(self.handle(), self.ix()));
    }
    /// stat_compound_assign: the right-hand value.
    pub fn compoundRhs(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_compound_rhs(self.handle(), self.ix()));
    }

    /// stat_function: the name expression (`a.b.c`).
    pub fn statFunctionName(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_stat_function_name(self.handle(), self.ix()));
    }
    /// stat_function: the function expression.
    pub fn statFunctionFunc(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_stat_function_func(self.handle(), self.ix()));
    }

    /// stat_local_function: the function name.
    pub fn localFunctionName(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_local_function_name(self.handle(), self.ix()));
    }
    /// stat_local_function: the function expression.
    pub fn localFunctionFunc(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_local_function_func(self.handle(), self.ix()));
    }

    // ---- type-annotation accessors ------------------------------------------

    /// stat_type_alias: the alias name (`type Name = ...`).
    pub fn typeAliasName(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_type_alias_name(self.handle(), self.ix()));
    }
    /// stat_type_alias: whether it is `export type`.
    pub fn typeAliasExported(self: Node) bool {
        return c.luau_ast_type_alias_exported(self.handle(), self.ix()) != 0;
    }
    /// stat_type_alias: the aliased type node.
    pub fn typeAliasType(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_type_alias_type(self.handle(), self.ix()));
    }
    /// stat_type_alias: number of generic type parameters.
    pub fn typeAliasGenericCount(self: Node) usize {
        return @intCast(c.luau_ast_type_alias_generic_count(self.handle(), self.ix()));
    }
    /// stat_type_alias: the `j`-th generic parameter name.
    pub fn typeAliasGenericName(self: Node, j: usize) []const u8 {
        return std.mem.span(c.luau_ast_type_alias_generic_name(self.handle(), self.ix(), @intCast(j)));
    }

    /// type_reference: module prefix (`mod.Type`), or "" if none.
    pub fn typeReferencePrefix(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_type_reference_prefix(self.handle(), self.ix()));
    }
    /// type_reference: the referenced type name.
    pub fn typeReferenceName(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_type_reference_name(self.handle(), self.ix()));
    }
    /// type_reference: number of type arguments (`Type<A, B>`).
    pub fn typeReferenceParamCount(self: Node) usize {
        return @intCast(c.luau_ast_type_reference_param_count(self.handle(), self.ix()));
    }
    /// type_reference: the `j`-th type argument, or null if it is a type pack.
    pub fn typeReferenceParam(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_type_reference_param(self.handle(), self.ix(), @intCast(j)));
    }

    /// type_union: number of members (`A | B | C`).
    pub fn typeUnionCount(self: Node) usize {
        return @intCast(c.luau_ast_type_union_count(self.handle(), self.ix()));
    }
    /// type_union: the `j`-th member type.
    pub fn typeUnionMember(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_type_union_member(self.handle(), self.ix(), @intCast(j)));
    }
    /// type_intersection: number of members (`A & B & C`).
    pub fn typeIntersectionCount(self: Node) usize {
        return @intCast(c.luau_ast_type_intersection_count(self.handle(), self.ix()));
    }
    /// type_intersection: the `j`-th member type.
    pub fn typeIntersectionMember(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_type_intersection_member(self.handle(), self.ix(), @intCast(j)));
    }

    /// type_singleton_string: the string literal value (`"foo"`).
    pub fn typeSingletonStringValue(self: Node) []const u8 {
        return std.mem.span(c.luau_ast_type_singleton_string_value(self.handle(), self.ix()));
    }

    /// type_table: number of properties.
    pub fn typeTablePropCount(self: Node) usize {
        return @intCast(c.luau_ast_type_table_prop_count(self.handle(), self.ix()));
    }
    /// type_table: the `j`-th property name.
    pub fn typeTablePropName(self: Node, j: usize) []const u8 {
        return std.mem.span(c.luau_ast_type_table_prop_name(self.handle(), self.ix(), @intCast(j)));
    }
    /// type_table: the `j`-th property type.
    pub fn typeTablePropType(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_type_table_prop_type(self.handle(), self.ix(), @intCast(j)));
    }
    /// type_table: the `j`-th property access modifier.
    pub fn typeTablePropAccess(self: Node, j: usize) TableAccess {
        return @enumFromInt(c.luau_ast_type_table_prop_access(self.handle(), self.ix(), @intCast(j)));
    }

    /// type_typeof: the expression inside `typeof(...)`.
    pub fn typeofExpr(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_type_typeof_expr(self.handle(), self.ix()));
    }
    /// type_group: the parenthesized inner type.
    pub fn typeGroupType(self: Node) ?Node {
        return self.nodeAt(c.luau_ast_type_group_type(self.handle(), self.ix()));
    }

    /// expr_function: the `j`-th parameter's type annotation, or null if none.
    /// Pairs with `functionParamName`.
    pub fn functionParamAnnotation(self: Node, j: usize) ?Node {
        return self.nodeAt(c.luau_ast_function_param_annotation(self.handle(), self.ix(), @intCast(j)));
    }
};

/// A table-type property's access modifier (`AstTableAccess`).
pub const TableAccess = enum(c_int) { read_write = 0, read = 1, write = 2, _ };

/// The kind of a table constructor item (`expr_table`).
pub const TableItemKind = enum(c_int) { list = 0, record = 1, general = 2, _ };

pub const ChildIterator = struct {
    parsed: Parsed,
    parent_index: usize,
    i: usize,
    pub fn next(it: *ChildIterator) ?Node {
        const total = it.parsed.nodeCount();
        while (it.i < total) : (it.i += 1) {
            const p = c.luau_ast_node_parent(it.parsed.handle, @intCast(it.i));
            if (p >= 0 and @as(usize, @intCast(p)) == it.parent_index) {
                defer it.i += 1;
                return .{ .parsed = it.parsed, .index = it.i };
            }
        }
        return null;
    }
};

/// Parse Luau `source` into an AST. Never fails to *return*; inspect `.ok()` /
/// `.errors()` for syntax errors.
pub fn parse(source: []const u8) Parsed {
    return .{ .handle = c.luau_ast_parse(source.ptr, source.len).? };
}
