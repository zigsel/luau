// extern "C" shim over Luau::Parser (Ast module).

#include "ast.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/Parser.h"

#include <string>
#include <unordered_map>
#include <vector>

using namespace Luau;

// One flattened AST node.
struct FlatNode {
    LuauAstKind kind;
    int parent;
    Position begin;
    Position end;
    std::string str;
    double number;
    long long integer;
    int boolean;
    AstNode* ast; // the underlying typed node, for typed field accessors

    FlatNode(LuauAstKind k, int p, Location loc, AstNode* ast)
        : kind(k), parent(p), begin(loc.begin), end(loc.end)
        , number(0), integer(0), boolean(0), ast(ast) {}
};

namespace {
// forward decl of the recursive flattener (defined after LuauParseResult)
struct Walker;
}

struct LuauParseResult {
    Allocator allocator;
    AstNameTable names;
    ParseResult result;
    std::vector<std::string> errorMessages;
    std::vector<std::string> hotcommentContent;
    std::vector<FlatNode> nodes;
    std::unordered_map<const AstNode*, int> index; // AstNode* -> flat index

    LuauParseResult()
        : allocator()
        , names(allocator)
    {
    }
};

namespace {

static bool contains(const Location& outer, const Location& inner) {
    return outer.begin <= inner.begin && inner.end <= outer.end;
}

// Depth-first flattener. AstVisitor has no post-visit hook, so parents are
// recovered from source-location nesting: in a pre-order DFS a node's parent is
// the nearest still-open enclosing node. We keep a stack of (flatIndex, span)
// and pop entries that no longer contain the current node.
struct Collector : AstVisitor {
    std::vector<FlatNode>& out;
    std::unordered_map<const AstNode*, int>& index;
    std::vector<std::pair<int, Location>> stack;
    Collector(std::vector<FlatNode>& out, std::unordered_map<const AstNode*, int>& index)
        : out(out), index(index) {}

    int add(LuauAstKind kind, AstNode* node) {
        while (!stack.empty() && !contains(stack.back().second, node->location))
            stack.pop_back();
        int idx = static_cast<int>(out.size());
        FlatNode fn(kind, stack.empty() ? -1 : stack.back().first, node->location, node);
        out.push_back(fn);
        index[node] = idx;
        stack.push_back({idx, node->location});
        return idx;
    }

    // statements
    bool visit(AstStatBlock* n) override { add(LUAU_AST_STAT_BLOCK, n); return true; }
    bool visit(AstStatIf* n) override { add(LUAU_AST_STAT_IF, n); return true; }
    bool visit(AstStatWhile* n) override { add(LUAU_AST_STAT_WHILE, n); return true; }
    bool visit(AstStatRepeat* n) override { add(LUAU_AST_STAT_REPEAT, n); return true; }
    bool visit(AstStatBreak* n) override { add(LUAU_AST_STAT_BREAK, n); return true; }
    bool visit(AstStatContinue* n) override { add(LUAU_AST_STAT_CONTINUE, n); return true; }
    bool visit(AstStatReturn* n) override { add(LUAU_AST_STAT_RETURN, n); return true; }
    bool visit(AstStatExpr* n) override { add(LUAU_AST_STAT_EXPR, n); return true; }
    bool visit(AstStatFor* n) override { add(LUAU_AST_STAT_FOR, n); return true; }
    bool visit(AstStatForIn* n) override { add(LUAU_AST_STAT_FOR_IN, n); return true; }
    bool visit(AstStatAssign* n) override { add(LUAU_AST_STAT_ASSIGN, n); return true; }
    bool visit(AstStatCompoundAssign* n) override { add(LUAU_AST_STAT_COMPOUND_ASSIGN, n); return true; }
    bool visit(AstStatTypeAlias* n) override { add(LUAU_AST_STAT_TYPE_ALIAS, n); return true; }
    bool visit(AstStatError* n) override { add(LUAU_AST_STAT_ERROR, n); return true; }
    bool visit(AstStatFunction* n) override { add(LUAU_AST_STAT_FUNCTION, n); return true; }
    bool visit(AstStatLocal* n) override {
        int idx = add(LUAU_AST_STAT_LOCAL, n);
        if (n->vars.size > 0) out[idx].str = std::string(n->vars.data[0]->name.value);
        return true;
    }
    bool visit(AstStatLocalFunction* n) override {
        int idx = add(LUAU_AST_STAT_LOCAL_FUNCTION, n);
        if (n->name) out[idx].str = std::string(n->name->name.value);
        return true;
    }

    // expressions
    bool visit(AstExprGroup* n) override { add(LUAU_AST_EXPR_GROUP, n); return true; }
    bool visit(AstExprConstantNil* n) override { add(LUAU_AST_EXPR_CONSTANT_NIL, n); return true; }
    bool visit(AstExprConstantBool* n) override {
        out[add(LUAU_AST_EXPR_CONSTANT_BOOL, n)].boolean = n->value ? 1 : 0; return true;
    }
    bool visit(AstExprConstantNumber* n) override {
        out[add(LUAU_AST_EXPR_CONSTANT_NUMBER, n)].number = n->value; return true;
    }
    bool visit(AstExprConstantString* n) override {
        out[add(LUAU_AST_EXPR_CONSTANT_STRING, n)].str = std::string(n->value.data, n->value.size); return true;
    }
    bool visit(AstExprLocal* n) override {
        out[add(LUAU_AST_EXPR_LOCAL, n)].str = std::string(n->local->name.value); return true;
    }
    bool visit(AstExprGlobal* n) override {
        out[add(LUAU_AST_EXPR_GLOBAL, n)].str = std::string(n->name.value); return true;
    }
    bool visit(AstExprVarargs* n) override { add(LUAU_AST_EXPR_VARARGS, n); return true; }
    bool visit(AstExprCall* n) override { add(LUAU_AST_EXPR_CALL, n); return true; }
    bool visit(AstExprIndexName* n) override {
        out[add(LUAU_AST_EXPR_INDEX_NAME, n)].str = std::string(n->index.value); return true;
    }
    bool visit(AstExprIndexExpr* n) override { add(LUAU_AST_EXPR_INDEX_EXPR, n); return true; }
    bool visit(AstExprFunction* n) override { add(LUAU_AST_EXPR_FUNCTION, n); return true; }
    bool visit(AstExprTable* n) override { add(LUAU_AST_EXPR_TABLE, n); return true; }
    bool visit(AstExprUnary* n) override {
        out[add(LUAU_AST_EXPR_UNARY, n)].integer = static_cast<long long>(n->op); return true;
    }
    bool visit(AstExprBinary* n) override {
        out[add(LUAU_AST_EXPR_BINARY, n)].integer = static_cast<long long>(n->op); return true;
    }
    bool visit(AstExprTypeAssertion* n) override { add(LUAU_AST_EXPR_TYPE_ASSERTION, n); return true; }
    bool visit(AstExprIfElse* n) override { add(LUAU_AST_EXPR_IF_ELSE, n); return true; }
    bool visit(AstExprInterpString* n) override { add(LUAU_AST_EXPR_INTERP_STRING, n); return true; }
    bool visit(AstExprError* n) override { add(LUAU_AST_EXPR_ERROR, n); return true; }

    // types
    bool visit(AstTypeReference* n) override {
        out[add(LUAU_AST_TYPE_REFERENCE, n)].str = std::string(n->name.value); return true;
    }
    bool visit(AstTypeTable* n) override { add(LUAU_AST_TYPE_TABLE, n); return true; }
    bool visit(AstTypeFunction* n) override { add(LUAU_AST_TYPE_FUNCTION, n); return true; }
    bool visit(AstTypeTypeof* n) override { add(LUAU_AST_TYPE_TYPEOF, n); return true; }
    bool visit(AstTypeUnion* n) override { add(LUAU_AST_TYPE_UNION, n); return true; }
    bool visit(AstTypeIntersection* n) override { add(LUAU_AST_TYPE_INTERSECTION, n); return true; }
    bool visit(AstTypeGroup* n) override { add(LUAU_AST_TYPE_GROUP, n); return true; }
    bool visit(AstTypeOptional* n) override { add(LUAU_AST_TYPE_OPTIONAL, n); return true; }
    bool visit(AstTypeSingletonString* n) override {
        out[add(LUAU_AST_TYPE_SINGLETON_STRING, n)].str = std::string(n->value.data, n->value.size); return true;
    }
    bool visit(AstTypeError* n) override { add(LUAU_AST_TYPE_ERROR, n); return true; }

    bool visit(AstNode*) override { return true; }
};

} // namespace

extern "C" LuauParseResult* luau_ast_parse(const char* src, size_t len) {
    LuauParseResult* r = new LuauParseResult();
    try {
        ParseOptions options;
        r->result = Parser::parse(src, len, r->names, r->allocator, options);
    } catch (const std::exception& e) {
        // Catastrophic parse failure: surface as a single error.
        r->errorMessages.push_back(e.what());
        return r;
    }
    for (const ParseError& e : r->result.errors)
        r->errorMessages.push_back(e.getMessage());
    for (const HotComment& h : r->result.hotcomments)
        r->hotcommentContent.push_back(h.content);
    if (r->result.root) {
        Collector collector(r->nodes, r->index);
        r->result.root->visit(&collector);
    }
    return r;
}

extern "C" int luau_ast_error_count(const LuauParseResult* r) {
    return static_cast<int>(r->errorMessages.size());
}

extern "C" const char* luau_ast_error_message(const LuauParseResult* r, int i) {
    if (i < 0 || static_cast<size_t>(i) >= r->errorMessages.size())
        return "";
    return r->errorMessages[i].c_str();
}

extern "C" LuauPosition luau_ast_error_position(const LuauParseResult* r, int i) {
    LuauPosition p = {0, 0};
    if (i >= 0 && static_cast<size_t>(i) < r->result.errors.size()) {
        Position begin = r->result.errors[i].getLocation().begin;
        p.line = begin.line;
        p.column = begin.column;
    }
    return p;
}

extern "C" size_t luau_ast_line_count(const LuauParseResult* r) {
    return r->result.lines;
}

extern "C" int luau_ast_has_root(const LuauParseResult* r) {
    return r->result.root != nullptr ? 1 : 0;
}

extern "C" int luau_ast_hotcomment_count(const LuauParseResult* r) {
    return static_cast<int>(r->hotcommentContent.size());
}

extern "C" const char* luau_ast_hotcomment_content(const LuauParseResult* r, int i) {
    if (i < 0 || static_cast<size_t>(i) >= r->hotcommentContent.size())
        return "";
    return r->hotcommentContent[i].c_str();
}

extern "C" void luau_ast_parse_free(LuauParseResult* r) {
    delete r;
}

// ---- node walking ----------------------------------------------------------

static const FlatNode* nodeAt(const LuauParseResult* r, int i) {
    if (i < 0 || static_cast<size_t>(i) >= r->nodes.size()) return nullptr;
    return &r->nodes[i];
}

extern "C" int luau_ast_node_count(const LuauParseResult* r) {
    return static_cast<int>(r->nodes.size());
}
extern "C" int luau_ast_node_kind(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? static_cast<int>(n->kind) : LUAU_AST_UNKNOWN;
}
extern "C" int luau_ast_node_parent(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->parent : -1;
}
extern "C" LuauPosition luau_ast_node_begin(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    LuauPosition p = {0, 0};
    if (n) { p.line = n->begin.line; p.column = n->begin.column; }
    return p;
}
extern "C" LuauPosition luau_ast_node_end(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    LuauPosition p = {0, 0};
    if (n) { p.line = n->end.line; p.column = n->end.column; }
    return p;
}
extern "C" const char* luau_ast_node_string(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->str.c_str() : "";
}
extern "C" double luau_ast_node_number(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->number : 0.0;
}
extern "C" long long luau_ast_node_integer(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->integer : 0;
}
extern "C" int luau_ast_node_boolean(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->boolean : 0;
}

// ---- typed field accessors --------------------------------------------------
//
// Given a flat node index, return named child node indices (or -1) and scalar
// fields, casting the stored AstNode* to its concrete type. Child node pointers
// are resolved back to flat indices through the parse's pointer->index map; a
// pointer that was never flattened (or null) yields -1.

namespace {

// The concrete AstNode* for flat index `i`, or nullptr.
static AstNode* astAt(const LuauParseResult* r, int i) {
    const FlatNode* n = nodeAt(r, i);
    return n ? n->ast : nullptr;
}

// Resolve a child AstNode* to its flat index, or -1.
static int idxOf(const LuauParseResult* r, const AstNode* p) {
    if (!p) return -1;
    auto it = r->index.find(p);
    return it == r->index.end() ? -1 : it->second;
}

// Cast helper: the typed node at flat index `i`, or nullptr if kind mismatches.
template<typename T>
static T* as(const LuauParseResult* r, int i) {
    AstNode* n = astAt(r, i);
    return n ? n->as<T>() : nullptr;
}

} // namespace

// AstExprBinary: op (int), left, right.
extern "C" int luau_ast_binary_op(const LuauParseResult* r, int i) {
    AstExprBinary* n = as<AstExprBinary>(r, i);
    return n ? static_cast<int>(n->op) : -1;
}
extern "C" int luau_ast_binary_left(const LuauParseResult* r, int i) {
    AstExprBinary* n = as<AstExprBinary>(r, i);
    return n ? idxOf(r, n->left) : -1;
}
extern "C" int luau_ast_binary_right(const LuauParseResult* r, int i) {
    AstExprBinary* n = as<AstExprBinary>(r, i);
    return n ? idxOf(r, n->right) : -1;
}

// AstExprUnary: op (int), operand.
extern "C" int luau_ast_unary_op(const LuauParseResult* r, int i) {
    AstExprUnary* n = as<AstExprUnary>(r, i);
    return n ? static_cast<int>(n->op) : -1;
}
extern "C" int luau_ast_unary_operand(const LuauParseResult* r, int i) {
    AstExprUnary* n = as<AstExprUnary>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}

// AstExprGroup: expr.
extern "C" int luau_ast_group_expr(const LuauParseResult* r, int i) {
    AstExprGroup* n = as<AstExprGroup>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}

// AstExprCall: func, self (bool), arg count + arg(i).
extern "C" int luau_ast_call_func(const LuauParseResult* r, int i) {
    AstExprCall* n = as<AstExprCall>(r, i);
    return n ? idxOf(r, n->func) : -1;
}
extern "C" int luau_ast_call_self(const LuauParseResult* r, int i) {
    AstExprCall* n = as<AstExprCall>(r, i);
    return n ? (n->self ? 1 : 0) : 0;
}
extern "C" int luau_ast_call_arg_count(const LuauParseResult* r, int i) {
    AstExprCall* n = as<AstExprCall>(r, i);
    return n ? static_cast<int>(n->args.size) : 0;
}
extern "C" int luau_ast_call_arg(const LuauParseResult* r, int i, int j) {
    AstExprCall* n = as<AstExprCall>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->args.size) return -1;
    return idxOf(r, n->args.data[j]);
}

// AstExprIndexName: expr, index name (string).
extern "C" int luau_ast_index_name_expr(const LuauParseResult* r, int i) {
    AstExprIndexName* n = as<AstExprIndexName>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}
extern "C" const char* luau_ast_index_name_index(const LuauParseResult* r, int i) {
    AstExprIndexName* n = as<AstExprIndexName>(r, i);
    return n ? n->index.value : "";
}

// AstExprIndexExpr: expr, index.
extern "C" int luau_ast_index_expr_expr(const LuauParseResult* r, int i) {
    AstExprIndexExpr* n = as<AstExprIndexExpr>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}
extern "C" int luau_ast_index_expr_index(const LuauParseResult* r, int i) {
    AstExprIndexExpr* n = as<AstExprIndexExpr>(r, i);
    return n ? idxOf(r, n->index) : -1;
}

// AstExprFunction: param count + param name(i), vararg (bool), body.
extern "C" int luau_ast_function_param_count(const LuauParseResult* r, int i) {
    AstExprFunction* n = as<AstExprFunction>(r, i);
    return n ? static_cast<int>(n->args.size) : 0;
}
extern "C" const char* luau_ast_function_param_name(const LuauParseResult* r, int i, int j) {
    AstExprFunction* n = as<AstExprFunction>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->args.size) return "";
    return n->args.data[j]->name.value;
}
extern "C" int luau_ast_function_vararg(const LuauParseResult* r, int i) {
    AstExprFunction* n = as<AstExprFunction>(r, i);
    return n ? (n->vararg ? 1 : 0) : 0;
}
extern "C" int luau_ast_function_body(const LuauParseResult* r, int i) {
    AstExprFunction* n = as<AstExprFunction>(r, i);
    return n ? idxOf(r, n->body) : -1;
}

// AstExprTable: item count + item kind/key/value(i).
extern "C" int luau_ast_table_item_count(const LuauParseResult* r, int i) {
    AstExprTable* n = as<AstExprTable>(r, i);
    return n ? static_cast<int>(n->items.size) : 0;
}
extern "C" int luau_ast_table_item_kind(const LuauParseResult* r, int i, int j) {
    AstExprTable* n = as<AstExprTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->items.size) return -1;
    return static_cast<int>(n->items.data[j].kind);
}
extern "C" int luau_ast_table_item_key(const LuauParseResult* r, int i, int j) {
    AstExprTable* n = as<AstExprTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->items.size) return -1;
    return idxOf(r, n->items.data[j].key);
}
extern "C" int luau_ast_table_item_value(const LuauParseResult* r, int i, int j) {
    AstExprTable* n = as<AstExprTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->items.size) return -1;
    return idxOf(r, n->items.data[j].value);
}

// AstExprTypeAssertion: expr, annotation.
extern "C" int luau_ast_type_assertion_expr(const LuauParseResult* r, int i) {
    AstExprTypeAssertion* n = as<AstExprTypeAssertion>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}
extern "C" int luau_ast_type_assertion_annotation(const LuauParseResult* r, int i) {
    AstExprTypeAssertion* n = as<AstExprTypeAssertion>(r, i);
    return n ? idxOf(r, n->annotation) : -1;
}

// AstExprIfElse: condition, trueexpr, falseexpr.
extern "C" int luau_ast_ifelse_condition(const LuauParseResult* r, int i) {
    AstExprIfElse* n = as<AstExprIfElse>(r, i);
    return n ? idxOf(r, n->condition) : -1;
}
extern "C" int luau_ast_ifelse_trueexpr(const LuauParseResult* r, int i) {
    AstExprIfElse* n = as<AstExprIfElse>(r, i);
    return n ? idxOf(r, n->trueExpr) : -1;
}
extern "C" int luau_ast_ifelse_falseexpr(const LuauParseResult* r, int i) {
    AstExprIfElse* n = as<AstExprIfElse>(r, i);
    return n ? idxOf(r, n->falseExpr) : -1;
}

// AstExprInterpString: expr count + expr(i).
extern "C" int luau_ast_interp_expr_count(const LuauParseResult* r, int i) {
    AstExprInterpString* n = as<AstExprInterpString>(r, i);
    return n ? static_cast<int>(n->expressions.size) : 0;
}
extern "C" int luau_ast_interp_expr(const LuauParseResult* r, int i, int j) {
    AstExprInterpString* n = as<AstExprInterpString>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->expressions.size) return -1;
    return idxOf(r, n->expressions.data[j]);
}

// AstStatBlock: stat count + stat(i).
extern "C" int luau_ast_block_stat_count(const LuauParseResult* r, int i) {
    AstStatBlock* n = as<AstStatBlock>(r, i);
    return n ? static_cast<int>(n->body.size) : 0;
}
extern "C" int luau_ast_block_stat(const LuauParseResult* r, int i, int j) {
    AstStatBlock* n = as<AstStatBlock>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->body.size) return -1;
    return idxOf(r, n->body.data[j]);
}

// AstStatIf: condition, thenbody, elsebody.
extern "C" int luau_ast_if_condition(const LuauParseResult* r, int i) {
    AstStatIf* n = as<AstStatIf>(r, i);
    return n ? idxOf(r, n->condition) : -1;
}
extern "C" int luau_ast_if_thenbody(const LuauParseResult* r, int i) {
    AstStatIf* n = as<AstStatIf>(r, i);
    return n ? idxOf(r, n->thenbody) : -1;
}
extern "C" int luau_ast_if_elsebody(const LuauParseResult* r, int i) {
    AstStatIf* n = as<AstStatIf>(r, i);
    return n ? idxOf(r, n->elsebody) : -1;
}

// AstStatWhile: condition, body.
extern "C" int luau_ast_while_condition(const LuauParseResult* r, int i) {
    AstStatWhile* n = as<AstStatWhile>(r, i);
    return n ? idxOf(r, n->condition) : -1;
}
extern "C" int luau_ast_while_body(const LuauParseResult* r, int i) {
    AstStatWhile* n = as<AstStatWhile>(r, i);
    return n ? idxOf(r, n->body) : -1;
}

// AstStatRepeat: condition, body.
extern "C" int luau_ast_repeat_condition(const LuauParseResult* r, int i) {
    AstStatRepeat* n = as<AstStatRepeat>(r, i);
    return n ? idxOf(r, n->condition) : -1;
}
extern "C" int luau_ast_repeat_body(const LuauParseResult* r, int i) {
    AstStatRepeat* n = as<AstStatRepeat>(r, i);
    return n ? idxOf(r, n->body) : -1;
}

// AstStatFor: var (name), from, to, step, body.
extern "C" const char* luau_ast_for_var(const LuauParseResult* r, int i) {
    AstStatFor* n = as<AstStatFor>(r, i);
    return (n && n->var) ? n->var->name.value : "";
}
extern "C" int luau_ast_for_from(const LuauParseResult* r, int i) {
    AstStatFor* n = as<AstStatFor>(r, i);
    return n ? idxOf(r, n->from) : -1;
}
extern "C" int luau_ast_for_to(const LuauParseResult* r, int i) {
    AstStatFor* n = as<AstStatFor>(r, i);
    return n ? idxOf(r, n->to) : -1;
}
extern "C" int luau_ast_for_step(const LuauParseResult* r, int i) {
    AstStatFor* n = as<AstStatFor>(r, i);
    return n ? idxOf(r, n->step) : -1;
}
extern "C" int luau_ast_for_body(const LuauParseResult* r, int i) {
    AstStatFor* n = as<AstStatFor>(r, i);
    return n ? idxOf(r, n->body) : -1;
}

// AstStatForIn: var count + var(i) name, value count + value(i), body.
extern "C" int luau_ast_forin_var_count(const LuauParseResult* r, int i) {
    AstStatForIn* n = as<AstStatForIn>(r, i);
    return n ? static_cast<int>(n->vars.size) : 0;
}
extern "C" const char* luau_ast_forin_var(const LuauParseResult* r, int i, int j) {
    AstStatForIn* n = as<AstStatForIn>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->vars.size) return "";
    return n->vars.data[j]->name.value;
}
extern "C" int luau_ast_forin_value_count(const LuauParseResult* r, int i) {
    AstStatForIn* n = as<AstStatForIn>(r, i);
    return n ? static_cast<int>(n->values.size) : 0;
}
extern "C" int luau_ast_forin_value(const LuauParseResult* r, int i, int j) {
    AstStatForIn* n = as<AstStatForIn>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->values.size) return -1;
    return idxOf(r, n->values.data[j]);
}
extern "C" int luau_ast_forin_body(const LuauParseResult* r, int i) {
    AstStatForIn* n = as<AstStatForIn>(r, i);
    return n ? idxOf(r, n->body) : -1;
}

// AstStatReturn: expr count + expr(i).
extern "C" int luau_ast_return_expr_count(const LuauParseResult* r, int i) {
    AstStatReturn* n = as<AstStatReturn>(r, i);
    return n ? static_cast<int>(n->list.size) : 0;
}
extern "C" int luau_ast_return_expr(const LuauParseResult* r, int i, int j) {
    AstStatReturn* n = as<AstStatReturn>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->list.size) return -1;
    return idxOf(r, n->list.data[j]);
}

// AstStatExpr: expr.
extern "C" int luau_ast_stat_expr_expr(const LuauParseResult* r, int i) {
    AstStatExpr* n = as<AstStatExpr>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}

// AstStatLocal: var count + var name(i), value count + value(i).
extern "C" int luau_ast_local_var_count(const LuauParseResult* r, int i) {
    AstStatLocal* n = as<AstStatLocal>(r, i);
    return n ? static_cast<int>(n->vars.size) : 0;
}
extern "C" const char* luau_ast_local_var_name(const LuauParseResult* r, int i, int j) {
    AstStatLocal* n = as<AstStatLocal>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->vars.size) return "";
    return n->vars.data[j]->name.value;
}
extern "C" int luau_ast_local_value_count(const LuauParseResult* r, int i) {
    AstStatLocal* n = as<AstStatLocal>(r, i);
    return n ? static_cast<int>(n->values.size) : 0;
}
extern "C" int luau_ast_local_value(const LuauParseResult* r, int i, int j) {
    AstStatLocal* n = as<AstStatLocal>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->values.size) return -1;
    return idxOf(r, n->values.data[j]);
}

// AstStatAssign: lhs count + lhs(i), rhs count + rhs(i).
extern "C" int luau_ast_assign_lhs_count(const LuauParseResult* r, int i) {
    AstStatAssign* n = as<AstStatAssign>(r, i);
    return n ? static_cast<int>(n->vars.size) : 0;
}
extern "C" int luau_ast_assign_lhs(const LuauParseResult* r, int i, int j) {
    AstStatAssign* n = as<AstStatAssign>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->vars.size) return -1;
    return idxOf(r, n->vars.data[j]);
}
extern "C" int luau_ast_assign_rhs_count(const LuauParseResult* r, int i) {
    AstStatAssign* n = as<AstStatAssign>(r, i);
    return n ? static_cast<int>(n->values.size) : 0;
}
extern "C" int luau_ast_assign_rhs(const LuauParseResult* r, int i, int j) {
    AstStatAssign* n = as<AstStatAssign>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->values.size) return -1;
    return idxOf(r, n->values.data[j]);
}

// AstStatCompoundAssign: op (int), lhs, rhs.
extern "C" int luau_ast_compound_op(const LuauParseResult* r, int i) {
    AstStatCompoundAssign* n = as<AstStatCompoundAssign>(r, i);
    return n ? static_cast<int>(n->op) : -1;
}
extern "C" int luau_ast_compound_lhs(const LuauParseResult* r, int i) {
    AstStatCompoundAssign* n = as<AstStatCompoundAssign>(r, i);
    return n ? idxOf(r, n->var) : -1;
}
extern "C" int luau_ast_compound_rhs(const LuauParseResult* r, int i) {
    AstStatCompoundAssign* n = as<AstStatCompoundAssign>(r, i);
    return n ? idxOf(r, n->value) : -1;
}

// AstStatFunction: name (expr), func.
extern "C" int luau_ast_stat_function_name(const LuauParseResult* r, int i) {
    AstStatFunction* n = as<AstStatFunction>(r, i);
    return n ? idxOf(r, n->name) : -1;
}
extern "C" int luau_ast_stat_function_func(const LuauParseResult* r, int i) {
    AstStatFunction* n = as<AstStatFunction>(r, i);
    return n ? idxOf(r, n->func) : -1;
}

// AstStatLocalFunction: name (string), func.
extern "C" const char* luau_ast_local_function_name(const LuauParseResult* r, int i) {
    AstStatLocalFunction* n = as<AstStatLocalFunction>(r, i);
    return (n && n->name) ? n->name->name.value : "";
}
extern "C" int luau_ast_local_function_func(const LuauParseResult* r, int i) {
    AstStatLocalFunction* n = as<AstStatLocalFunction>(r, i);
    return n ? idxOf(r, n->func) : -1;
}

// ---- type-annotation accessors -----------------------------------------------

extern "C" const char* luau_ast_type_alias_name(const LuauParseResult* r, int i) {
    AstStatTypeAlias* n = as<AstStatTypeAlias>(r, i);
    return n ? n->name.value : "";
}
extern "C" int luau_ast_type_alias_exported(const LuauParseResult* r, int i) {
    AstStatTypeAlias* n = as<AstStatTypeAlias>(r, i);
    return (n && n->exported) ? 1 : 0;
}
extern "C" int luau_ast_type_alias_type(const LuauParseResult* r, int i) {
    AstStatTypeAlias* n = as<AstStatTypeAlias>(r, i);
    return n ? idxOf(r, n->type) : -1;
}
extern "C" int luau_ast_type_alias_generic_count(const LuauParseResult* r, int i) {
    AstStatTypeAlias* n = as<AstStatTypeAlias>(r, i);
    return n ? static_cast<int>(n->generics.size) : 0;
}
extern "C" const char* luau_ast_type_alias_generic_name(const LuauParseResult* r, int i, int j) {
    AstStatTypeAlias* n = as<AstStatTypeAlias>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->generics.size) return "";
    return n->generics.data[j]->name.value;
}

extern "C" const char* luau_ast_type_reference_prefix(const LuauParseResult* r, int i) {
    AstTypeReference* n = as<AstTypeReference>(r, i);
    return (n && n->prefix) ? n->prefix->value : "";
}
extern "C" const char* luau_ast_type_reference_name(const LuauParseResult* r, int i) {
    AstTypeReference* n = as<AstTypeReference>(r, i);
    return n ? n->name.value : "";
}
extern "C" int luau_ast_type_reference_param_count(const LuauParseResult* r, int i) {
    AstTypeReference* n = as<AstTypeReference>(r, i);
    return n ? static_cast<int>(n->parameters.size) : 0;
}
extern "C" int luau_ast_type_reference_param(const LuauParseResult* r, int i, int j) {
    AstTypeReference* n = as<AstTypeReference>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->parameters.size) return -1;
    return idxOf(r, n->parameters.data[j].type); // -1 if the argument is a type pack
}

extern "C" int luau_ast_type_union_count(const LuauParseResult* r, int i) {
    AstTypeUnion* n = as<AstTypeUnion>(r, i);
    return n ? static_cast<int>(n->types.size) : 0;
}
extern "C" int luau_ast_type_union_member(const LuauParseResult* r, int i, int j) {
    AstTypeUnion* n = as<AstTypeUnion>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->types.size) return -1;
    return idxOf(r, n->types.data[j]);
}
extern "C" int luau_ast_type_intersection_count(const LuauParseResult* r, int i) {
    AstTypeIntersection* n = as<AstTypeIntersection>(r, i);
    return n ? static_cast<int>(n->types.size) : 0;
}
extern "C" int luau_ast_type_intersection_member(const LuauParseResult* r, int i, int j) {
    AstTypeIntersection* n = as<AstTypeIntersection>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->types.size) return -1;
    return idxOf(r, n->types.data[j]);
}

extern "C" const char* luau_ast_type_singleton_string_value(const LuauParseResult* r, int i) {
    AstTypeSingletonString* n = as<AstTypeSingletonString>(r, i);
    if (!n) return "";
    return r->nodes[i].str.c_str();
}

extern "C" int luau_ast_type_table_prop_count(const LuauParseResult* r, int i) {
    AstTypeTable* n = as<AstTypeTable>(r, i);
    return n ? static_cast<int>(n->props.size) : 0;
}
extern "C" const char* luau_ast_type_table_prop_name(const LuauParseResult* r, int i, int j) {
    AstTypeTable* n = as<AstTypeTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->props.size) return "";
    return n->props.data[j].name.value;
}
extern "C" int luau_ast_type_table_prop_type(const LuauParseResult* r, int i, int j) {
    AstTypeTable* n = as<AstTypeTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->props.size) return -1;
    return idxOf(r, n->props.data[j].type);
}
extern "C" int luau_ast_type_table_prop_access(const LuauParseResult* r, int i, int j) {
    AstTypeTable* n = as<AstTypeTable>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->props.size) return 0;
    return static_cast<int>(n->props.data[j].access);
}

extern "C" int luau_ast_type_typeof_expr(const LuauParseResult* r, int i) {
    AstTypeTypeof* n = as<AstTypeTypeof>(r, i);
    return n ? idxOf(r, n->expr) : -1;
}
extern "C" int luau_ast_type_group_type(const LuauParseResult* r, int i) {
    AstTypeGroup* n = as<AstTypeGroup>(r, i);
    return n ? idxOf(r, n->type) : -1;
}

extern "C" int luau_ast_function_param_annotation(const LuauParseResult* r, int i, int j) {
    AstExprFunction* n = as<AstExprFunction>(r, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->args.size) return -1;
    return idxOf(r, n->args.data[j]->annotation);
}
