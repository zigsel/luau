// extern "C" shim over Luau's CST capture (Ast module) for formatters/codemods.

#include "cst.h"
#include "ast.h" // LuauAstKind enum shared with the AST walker

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Cst.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"

#include <climits>
#include <string>
#include <vector>

using namespace Luau;

namespace {

// One trivia entry: a named token position (or an integer "info" value).
struct Trivia {
    std::string name;
    Position pos{0, 0};
    bool missing = false;
    long long value = 0;

    // Position::missing() is {UINT_MAX, UINT_MAX}; treat that as a missing token.
    Trivia(std::string n, Position p)
        : name(std::move(n)), pos(p), missing(p.line == UINT_MAX && p.column == UINT_MAX) {}
    // integer "info" entry (degenerate position)
    Trivia(std::string n, long long v) : name(std::move(n)), pos(0, 0), missing(true), value(v) {}
};

// One flattened node (parallels ast.cpp's FlatNode but CST-focused).
struct FlatNode {
    LuauAstKind kind;
    int parent;
    Position begin;
    Position end;
    AstNode* ast;          // key into cstNodeMap
    LuauCstKind cstKind;   // LUAU_CST_NONE if no/unknown CstNode
    std::vector<Trivia> trivia;

    FlatNode(LuauAstKind k, int p, Location loc, AstNode* a)
        : kind(k), parent(p), begin(loc.begin), end(loc.end), ast(a), cstKind(LUAU_CST_NONE) {}
};

} // namespace

struct LuauCst {
    Allocator allocator;
    AstNameTable names;
    ParseResult result;
    std::string source;
    std::vector<std::string> errorMessages;
    std::vector<std::string> hotcommentContent;
    std::vector<std::string> commentText;
    std::vector<FlatNode> nodes;

    LuauCst() : allocator(), names(allocator) {}
};

namespace {

static bool contains(const Location& outer, const Location& inner) {
    return outer.begin <= inner.begin && inner.end <= outer.end;
}

// Slice [begin,end) out of source by (line,column). Lines are 0-based.
static std::string slice(const std::string& src, Position begin, Position end) {
    size_t off = 0, line = 0, col = 0;
    size_t startOff = std::string::npos, endOff = std::string::npos;
    for (; off <= src.size(); ++off) {
        if (line == begin.line && col == begin.column && startOff == std::string::npos)
            startOff = off;
        if (line == end.line && col == end.column) { endOff = off; break; }
        if (off == src.size()) break;
        if (src[off] == '\n') { ++line; col = 0; } else ++col;
    }
    if (startOff == std::string::npos) return "";
    if (endOff == std::string::npos) endOff = src.size();
    if (endOff < startOff) return "";
    return src.substr(startOff, endOff - startOff);
}

static void addComma(std::vector<Trivia>& t, const char* base, const AstArray<Position>& arr) {
    for (size_t i = 0; i < arr.size; ++i)
        t.push_back(Trivia(std::string(base) + "[" + std::to_string(i) + "]", arr.data[i]));
}

// Decode a CstNode's scalar trivia into `out`. Sets `kind`. Nested-node and
// raw-buffer fields are intentionally omitted (see cst.h).
static void decode(CstNode* cst, LuauCstKind& kind, std::vector<Trivia>& out) {
    if (auto n = cst->as<CstAttr>()) {
        kind = LUAU_CST_ATTR;
        out.push_back(Trivia("hasAt", (long long)n->hasAt));
    } else if (auto n = cst->as<CstParametrizedAttr>()) {
        kind = LUAU_CST_PARAMETRIZED_ATTR;
        out.push_back(Trivia("openParen", n->openParenPosition));
        out.push_back(Trivia("closeParen", n->closeParenPosition));
        addComma(out, "argsComma", n->argsCommaPositions);
    } else if (auto n = cst->as<CstExprGroup>()) {
        kind = LUAU_CST_EXPR_GROUP;
        out.push_back(Trivia("close", n->closePosition));
    } else if (cst->as<CstExprConstantNumber>()) {
        kind = LUAU_CST_EXPR_CONSTANT_NUMBER; // raw value buffer omitted
    } else if (cst->as<CstExprConstantInteger>()) {
        kind = LUAU_CST_EXPR_CONSTANT_INTEGER; // raw value buffer omitted
    } else if (auto n = cst->as<CstExprConstantString>()) {
        kind = LUAU_CST_EXPR_CONSTANT_STRING; // sourceString buffer omitted
        out.push_back(Trivia("quoteStyle", (long long)n->quoteStyle));
        out.push_back(Trivia("blockDepth", (long long)n->blockDepth));
    } else if (auto n = cst->as<CstExprCall>()) {
        kind = LUAU_CST_EXPR_CALL;
        out.push_back(Trivia("openParens", n->openParens));
        out.push_back(Trivia("closeParens", n->closeParens));
        addComma(out, "comma", n->commaPositions);
    } else if (auto n = cst->as<CstExprIndexExpr>()) {
        kind = LUAU_CST_EXPR_INDEX_EXPR;
        out.push_back(Trivia("openBracket", n->openBracketPosition));
        out.push_back(Trivia("closeBracket", n->closeBracketPosition));
    } else if (auto n = cst->as<CstExprFunction>()) {
        kind = LUAU_CST_EXPR_FUNCTION; // attrLists (nested) omitted
        out.push_back(Trivia("functionKeyword", n->functionKeywordPosition));
        out.push_back(Trivia("openGenerics", n->openGenericsPosition));
        addComma(out, "genericsComma", n->genericsCommaPositions);
        out.push_back(Trivia("closeGenerics", n->closeGenericsPosition));
        addComma(out, "argsAnnotationColon", n->argsAnnotationColonPositions);
        addComma(out, "argsComma", n->argsCommaPositions);
        out.push_back(Trivia("varargAnnotationColon", n->varargAnnotationColonPosition));
        out.push_back(Trivia("returnSpecifier", n->returnSpecifierPosition));
    } else if (auto n = cst->as<CstExprTable>()) {
        kind = LUAU_CST_EXPR_TABLE;
        for (size_t i = 0; i < n->items.size; ++i) {
            const auto& it = n->items.data[i];
            std::string p = "item[" + std::to_string(i) + "].";
            out.push_back(Trivia(p + "indexerOpen", it.indexerOpenPosition));
            out.push_back(Trivia(p + "indexerClose", it.indexerClosePosition));
            out.push_back(Trivia(p + "equals", it.equalsPosition));
            out.push_back(Trivia(p + "separatorKind", (long long)it.separator));
            out.push_back(Trivia(p + "separator", it.separatorPosition));
        }
    } else if (auto n = cst->as<CstExprOp>()) {
        kind = LUAU_CST_EXPR_OP;
        out.push_back(Trivia("op", n->opPosition));
    } else if (auto n = cst->as<CstExprTypeAssertion>()) {
        kind = LUAU_CST_EXPR_TYPE_ASSERTION;
        out.push_back(Trivia("op", n->opPosition));
    } else if (auto n = cst->as<CstExprIfElse>()) {
        kind = LUAU_CST_EXPR_IF_ELSE;
        out.push_back(Trivia("then", n->thenPosition));
        out.push_back(Trivia("else", n->elsePosition));
        out.push_back(Trivia("isElseIf", (long long)n->isElseIf));
    } else if (auto n = cst->as<CstExprInterpString>()) {
        kind = LUAU_CST_EXPR_INTERP_STRING; // sourceStrings buffers omitted
        addComma(out, "stringPos", n->stringPositions);
    } else if (auto n = cst->as<CstStatDo>()) {
        kind = LUAU_CST_STAT_DO;
        out.push_back(Trivia("statsStart", n->statsStartPosition));
        out.push_back(Trivia("end", n->endPosition));
    } else if (auto n = cst->as<CstStatRepeat>()) {
        kind = LUAU_CST_STAT_REPEAT;
        out.push_back(Trivia("until", n->untilPosition));
    } else if (auto n = cst->as<CstStatReturn>()) {
        kind = LUAU_CST_STAT_RETURN;
        addComma(out, "comma", n->commaPositions);
    } else if (auto n = cst->as<CstStatLocal>()) {
        kind = LUAU_CST_STAT_LOCAL;
        addComma(out, "varsAnnotationColon", n->varsAnnotationColonPositions);
        addComma(out, "varsComma", n->varsCommaPositions);
        addComma(out, "valuesComma", n->valuesCommaPositions);
    } else if (auto n = cst->as<CstStatFor>()) {
        kind = LUAU_CST_STAT_FOR;
        out.push_back(Trivia("annotationColon", n->annotationColonPosition));
        out.push_back(Trivia("equals", n->equalsPosition));
        out.push_back(Trivia("endComma", n->endCommaPosition));
        out.push_back(Trivia("stepComma", n->stepCommaPosition));
    } else if (auto n = cst->as<CstStatForIn>()) {
        kind = LUAU_CST_STAT_FOR_IN;
        addComma(out, "varsAnnotationColon", n->varsAnnotationColonPositions);
        addComma(out, "varsComma", n->varsCommaPositions);
        addComma(out, "valuesComma", n->valuesCommaPositions);
    } else if (auto n = cst->as<CstStatAssign>()) {
        kind = LUAU_CST_STAT_ASSIGN;
        addComma(out, "varsComma", n->varsCommaPositions);
        out.push_back(Trivia("equals", n->equalsPosition));
        addComma(out, "valuesComma", n->valuesCommaPositions);
    } else if (auto n = cst->as<CstStatCompoundAssign>()) {
        kind = LUAU_CST_STAT_COMPOUND_ASSIGN;
        out.push_back(Trivia("op", n->opPosition));
    } else if (auto n = cst->as<CstStatFunction>()) {
        kind = LUAU_CST_STAT_FUNCTION; // attrLists (nested) omitted
        out.push_back(Trivia("functionKeyword", n->functionKeywordPosition));
    } else if (auto n = cst->as<CstStatLocalFunction>()) {
        kind = LUAU_CST_STAT_LOCAL_FUNCTION; // attrLists (nested) omitted
        out.push_back(Trivia("localKeyword", n->localKeywordPosition));
        out.push_back(Trivia("functionKeyword", n->functionKeywordPosition));
    } else if (auto n = cst->as<CstGenericType>()) {
        kind = LUAU_CST_GENERIC_TYPE;
        out.push_back(Trivia("defaultEquals", n->defaultEqualsPosition));
    } else if (auto n = cst->as<CstGenericTypePack>()) {
        kind = LUAU_CST_GENERIC_TYPE_PACK;
        out.push_back(Trivia("ellipsis", n->ellipsisPosition));
        out.push_back(Trivia("defaultEquals", n->defaultEqualsPosition));
    } else if (auto n = cst->as<CstStatTypeAlias>()) {
        kind = LUAU_CST_STAT_TYPE_ALIAS;
        out.push_back(Trivia("typeKeyword", n->typeKeywordPosition));
        out.push_back(Trivia("genericsOpen", n->genericsOpenPosition));
        addComma(out, "genericsComma", n->genericsCommaPositions);
        out.push_back(Trivia("genericsClose", n->genericsClosePosition));
        out.push_back(Trivia("equals", n->equalsPosition));
    } else if (auto n = cst->as<CstStatTypeFunction>()) {
        kind = LUAU_CST_STAT_TYPE_FUNCTION;
        out.push_back(Trivia("typeKeyword", n->typeKeywordPosition));
        out.push_back(Trivia("functionKeyword", n->functionKeywordPosition));
    } else if (auto n = cst->as<CstTypeReference>()) {
        kind = LUAU_CST_TYPE_REFERENCE;
        out.push_back(Trivia("prefixPoint", n->prefixPointPosition));
        out.push_back(Trivia("openParameters", n->openParametersPosition));
        addComma(out, "parametersComma", n->parametersCommaPositions);
        out.push_back(Trivia("closeParameters", n->closeParametersPosition));
    } else if (auto n = cst->as<CstTypeTable>()) {
        kind = LUAU_CST_TYPE_TABLE;
        out.push_back(Trivia("isArray", (long long)n->isArray));
        for (size_t i = 0; i < n->items.size; ++i) {
            const auto& it = n->items.data[i];
            std::string p = "item[" + std::to_string(i) + "].";
            out.push_back(Trivia(p + "kind", (long long)it.kind));
            out.push_back(Trivia(p + "indexerOpen", it.indexerOpenPosition));
            out.push_back(Trivia(p + "indexerClose", it.indexerClosePosition));
            out.push_back(Trivia(p + "colon", it.colonPosition));
            out.push_back(Trivia(p + "separatorKind", (long long)it.separator));
            out.push_back(Trivia(p + "separator", it.separatorPosition));
        }
    } else if (auto n = cst->as<CstTypeFunction>()) {
        kind = LUAU_CST_TYPE_FUNCTION;
        out.push_back(Trivia("openGenerics", n->openGenericsPosition));
        addComma(out, "genericsComma", n->genericsCommaPositions);
        out.push_back(Trivia("closeGenerics", n->closeGenericsPosition));
        out.push_back(Trivia("openArgs", n->openArgsPosition));
        addComma(out, "argumentNameColon", n->argumentNameColonPositions);
        addComma(out, "argumentsComma", n->argumentsCommaPositions);
        out.push_back(Trivia("closeArgs", n->closeArgsPosition));
        out.push_back(Trivia("returnArrow", n->returnArrowPosition));
    } else if (auto n = cst->as<CstTypeTypeof>()) {
        kind = LUAU_CST_TYPE_TYPEOF;
        out.push_back(Trivia("open", n->openPosition));
        out.push_back(Trivia("close", n->closePosition));
    } else if (auto n = cst->as<CstTypeUnion>()) {
        kind = LUAU_CST_TYPE_UNION;
        out.push_back(Trivia("leading", n->leadingPosition));
        addComma(out, "separator", n->separatorPositions);
    } else if (auto n = cst->as<CstTypeIntersection>()) {
        kind = LUAU_CST_TYPE_INTERSECTION;
        out.push_back(Trivia("leading", n->leadingPosition));
        addComma(out, "separator", n->separatorPositions);
    } else if (auto n = cst->as<CstTypeSingletonString>()) {
        kind = LUAU_CST_TYPE_SINGLETON_STRING; // sourceString buffer omitted
        out.push_back(Trivia("quoteStyle", (long long)n->quoteStyle));
        out.push_back(Trivia("blockDepth", (long long)n->blockDepth));
    } else if (auto n = cst->as<CstTypeGroup>()) {
        kind = LUAU_CST_TYPE_GROUP;
        out.push_back(Trivia("close", n->closePosition));
    } else if (auto n = cst->as<CstTypePackExplicit>()) {
        kind = LUAU_CST_TYPE_PACK_EXPLICIT;
        out.push_back(Trivia("openParentheses", n->openParenthesesPosition));
        out.push_back(Trivia("closeParentheses", n->closeParenthesesPosition));
        addComma(out, "comma", n->commaPositions);
    } else if (auto n = cst->as<CstTypePackGeneric>()) {
        kind = LUAU_CST_TYPE_PACK_GENERIC;
        out.push_back(Trivia("ellipsis", n->ellipsisPosition));
    } else {
        kind = LUAU_CST_OTHER; // a CstNode subclass we do not decode yet
    }
}

// Depth-first flattener, mirroring ast.cpp: parents are recovered from
// source-location nesting. We additionally record the AstNode pointer so the
// caller can look it up in the cstNodeMap after the walk.
struct Collector : AstVisitor {
    std::vector<FlatNode>& out;
    std::vector<std::pair<int, Location>> stack;
    explicit Collector(std::vector<FlatNode>& out) : out(out) {}

    int add(LuauAstKind kind, AstNode* node) {
        while (!stack.empty() && !contains(stack.back().second, node->location))
            stack.pop_back();
        int idx = static_cast<int>(out.size());
        out.push_back(FlatNode(kind, stack.empty() ? -1 : stack.back().first, node->location, node));
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
    bool visit(AstStatLocal* n) override { add(LUAU_AST_STAT_LOCAL, n); return true; }
    bool visit(AstStatLocalFunction* n) override { add(LUAU_AST_STAT_LOCAL_FUNCTION, n); return true; }

    // expressions
    bool visit(AstExprGroup* n) override { add(LUAU_AST_EXPR_GROUP, n); return true; }
    bool visit(AstExprConstantNil* n) override { add(LUAU_AST_EXPR_CONSTANT_NIL, n); return true; }
    bool visit(AstExprConstantBool* n) override { add(LUAU_AST_EXPR_CONSTANT_BOOL, n); return true; }
    bool visit(AstExprConstantNumber* n) override { add(LUAU_AST_EXPR_CONSTANT_NUMBER, n); return true; }
    bool visit(AstExprConstantString* n) override { add(LUAU_AST_EXPR_CONSTANT_STRING, n); return true; }
    bool visit(AstExprLocal* n) override { add(LUAU_AST_EXPR_LOCAL, n); return true; }
    bool visit(AstExprGlobal* n) override { add(LUAU_AST_EXPR_GLOBAL, n); return true; }
    bool visit(AstExprVarargs* n) override { add(LUAU_AST_EXPR_VARARGS, n); return true; }
    bool visit(AstExprCall* n) override { add(LUAU_AST_EXPR_CALL, n); return true; }
    bool visit(AstExprIndexName* n) override { add(LUAU_AST_EXPR_INDEX_NAME, n); return true; }
    bool visit(AstExprIndexExpr* n) override { add(LUAU_AST_EXPR_INDEX_EXPR, n); return true; }
    bool visit(AstExprFunction* n) override { add(LUAU_AST_EXPR_FUNCTION, n); return true; }
    bool visit(AstExprTable* n) override { add(LUAU_AST_EXPR_TABLE, n); return true; }
    bool visit(AstExprUnary* n) override { add(LUAU_AST_EXPR_UNARY, n); return true; }
    bool visit(AstExprBinary* n) override { add(LUAU_AST_EXPR_BINARY, n); return true; }
    bool visit(AstExprTypeAssertion* n) override { add(LUAU_AST_EXPR_TYPE_ASSERTION, n); return true; }
    bool visit(AstExprIfElse* n) override { add(LUAU_AST_EXPR_IF_ELSE, n); return true; }
    bool visit(AstExprInterpString* n) override { add(LUAU_AST_EXPR_INTERP_STRING, n); return true; }
    bool visit(AstExprError* n) override { add(LUAU_AST_EXPR_ERROR, n); return true; }

    // types
    bool visit(AstTypeReference* n) override { add(LUAU_AST_TYPE_REFERENCE, n); return true; }
    bool visit(AstTypeTable* n) override { add(LUAU_AST_TYPE_TABLE, n); return true; }
    bool visit(AstTypeFunction* n) override { add(LUAU_AST_TYPE_FUNCTION, n); return true; }
    bool visit(AstTypeTypeof* n) override { add(LUAU_AST_TYPE_TYPEOF, n); return true; }
    bool visit(AstTypeUnion* n) override { add(LUAU_AST_TYPE_UNION, n); return true; }
    bool visit(AstTypeIntersection* n) override { add(LUAU_AST_TYPE_INTERSECTION, n); return true; }
    bool visit(AstTypeGroup* n) override { add(LUAU_AST_TYPE_GROUP, n); return true; }
    bool visit(AstTypeError* n) override { add(LUAU_AST_TYPE_ERROR, n); return true; }

    bool visit(AstNode*) override { return true; }
};

static int commentKind(Lexeme::Type t) {
    switch (t) {
    case Lexeme::Comment: return LUAU_CST_COMMENT_LINE;
    case Lexeme::BlockComment: return LUAU_CST_COMMENT_BLOCK;
    default: return LUAU_CST_COMMENT_BROKEN;
    }
}

} // namespace

extern "C" LuauCst* luau_cst_parse(const char* src, size_t len) {
    LuauCst* c = new LuauCst();
    c->source.assign(src, len);
    try {
        ParseOptions options;
        options.captureComments = true;
        options.storeCstData = true;
        options.allowDeclarationSyntax = true;
        c->result = Parser::parse(src, len, c->names, c->allocator, options);
    } catch (const std::exception& e) {
        c->errorMessages.push_back(e.what());
        return c;
    }
    try {
        for (const ParseError& e : c->result.errors)
            c->errorMessages.push_back(e.getMessage());
        for (const HotComment& h : c->result.hotcomments)
            c->hotcommentContent.push_back(h.content);
        for (const Comment& cm : c->result.commentLocations)
            c->commentText.push_back(slice(c->source, cm.location.begin, cm.location.end));

        if (c->result.root) {
            Collector collector(c->nodes);
            c->result.root->visit(&collector);
            // Resolve each node's CstNode and decode its trivia.
            for (FlatNode& fn : c->nodes) {
                CstNode** found = c->result.cstNodeMap.find(fn.ast);
                if (found && *found)
                    decode(*found, fn.cstKind, fn.trivia);
            }
        }
    } catch (const std::exception& e) {
        c->errorMessages.push_back(e.what());
    }
    return c;
}

extern "C" void luau_cst_free(LuauCst* c) { delete c; }

// ---- diagnostics ----

extern "C" int luau_cst_error_count(const LuauCst* c) {
    return static_cast<int>(c->errorMessages.size());
}
extern "C" const char* luau_cst_error_message(const LuauCst* c, int i) {
    if (i < 0 || static_cast<size_t>(i) >= c->errorMessages.size()) return "";
    return c->errorMessages[i].c_str();
}
extern "C" LuauPosition luau_cst_error_position(const LuauCst* c, int i) {
    LuauPosition p = {0, 0};
    if (i >= 0 && static_cast<size_t>(i) < c->result.errors.size()) {
        Position b = c->result.errors[i].getLocation().begin;
        p.line = b.line; p.column = b.column;
    }
    return p;
}
extern "C" int luau_cst_has_root(const LuauCst* c) {
    return c->result.root != nullptr ? 1 : 0;
}
extern "C" size_t luau_cst_line_count(const LuauCst* c) { return c->result.lines; }

// ---- comments ----

extern "C" int luau_cst_comment_count(const LuauCst* c) {
    return static_cast<int>(c->result.commentLocations.size());
}
extern "C" int luau_cst_comment_kind(const LuauCst* c, int i) {
    if (i < 0 || static_cast<size_t>(i) >= c->result.commentLocations.size()) return LUAU_CST_COMMENT_BROKEN;
    return commentKind(c->result.commentLocations[i].type);
}
extern "C" LuauPosition luau_cst_comment_begin(const LuauCst* c, int i) {
    LuauPosition p = {0, 0};
    if (i >= 0 && static_cast<size_t>(i) < c->result.commentLocations.size()) {
        Position b = c->result.commentLocations[i].location.begin;
        p.line = b.line; p.column = b.column;
    }
    return p;
}
extern "C" LuauPosition luau_cst_comment_end(const LuauCst* c, int i) {
    LuauPosition p = {0, 0};
    if (i >= 0 && static_cast<size_t>(i) < c->result.commentLocations.size()) {
        Position e = c->result.commentLocations[i].location.end;
        p.line = e.line; p.column = e.column;
    }
    return p;
}
extern "C" const char* luau_cst_comment_text(const LuauCst* c, int i) {
    if (i < 0 || static_cast<size_t>(i) >= c->commentText.size()) return "";
    return c->commentText[i].c_str();
}
extern "C" int luau_cst_hotcomment_count(const LuauCst* c) {
    return static_cast<int>(c->hotcommentContent.size());
}
extern "C" const char* luau_cst_hotcomment_content(const LuauCst* c, int i) {
    if (i < 0 || static_cast<size_t>(i) >= c->hotcommentContent.size()) return "";
    return c->hotcommentContent[i].c_str();
}

// ---- node view ----

static const FlatNode* nodeAt(const LuauCst* c, int i) {
    if (i < 0 || static_cast<size_t>(i) >= c->nodes.size()) return nullptr;
    return &c->nodes[i];
}

extern "C" int luau_cst_node_count(const LuauCst* c) {
    return static_cast<int>(c->nodes.size());
}
extern "C" int luau_cst_node_kind(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    return n ? static_cast<int>(n->kind) : LUAU_AST_UNKNOWN;
}
extern "C" int luau_cst_node_parent(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    return n ? n->parent : -1;
}
extern "C" LuauPosition luau_cst_node_begin(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    LuauPosition p = {0, 0};
    if (n) { p.line = n->begin.line; p.column = n->begin.column; }
    return p;
}
extern "C" LuauPosition luau_cst_node_end(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    LuauPosition p = {0, 0};
    if (n) { p.line = n->end.line; p.column = n->end.column; }
    return p;
}
extern "C" int luau_cst_node_has_cst(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    return (n && n->cstKind != LUAU_CST_NONE) ? 1 : 0;
}
extern "C" int luau_cst_node_cst_kind(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    return n ? static_cast<int>(n->cstKind) : LUAU_CST_NONE;
}

// ---- trivia ----

static const Trivia* triviaAt(const LuauCst* c, int i, int j) {
    const FlatNode* n = nodeAt(c, i);
    if (!n || j < 0 || static_cast<size_t>(j) >= n->trivia.size()) return nullptr;
    return &n->trivia[j];
}
extern "C" int luau_cst_node_trivia_count(const LuauCst* c, int i) {
    const FlatNode* n = nodeAt(c, i);
    return n ? static_cast<int>(n->trivia.size()) : 0;
}
extern "C" const char* luau_cst_node_trivia_name(const LuauCst* c, int i, int j) {
    const Trivia* t = triviaAt(c, i, j);
    return t ? t->name.c_str() : "";
}
extern "C" LuauPosition luau_cst_node_trivia_position(const LuauCst* c, int i, int j) {
    const Trivia* t = triviaAt(c, i, j);
    LuauPosition p = {0, 0};
    if (t) { p.line = t->pos.line; p.column = t->pos.column; }
    return p;
}
extern "C" int luau_cst_node_trivia_missing(const LuauCst* c, int i, int j) {
    const Trivia* t = triviaAt(c, i, j);
    return (t && t->missing) ? 1 : 0;
}
extern "C" long long luau_cst_node_trivia_value(const LuauCst* c, int i, int j) {
    const Trivia* t = triviaAt(c, i, j);
    return t ? t->value : 0;
}
