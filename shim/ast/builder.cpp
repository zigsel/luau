// extern "C" shim over Luau AST construction + compilation (Ast + Compiler).

#include "builder.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Lexer.h"
#include "Luau/Location.h"
#include "Luau/ParseResult.h"
#include "Luau/PrettyPrinter.h"
#include "Luau/Cst.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cctype>
#include <exception>

using namespace Luau;

struct LuauAstBuilder {
    Allocator allocator;
    AstNameTable names{allocator};
};

namespace {

// A synthetic zero location shared by every constructed node.
const Location kLoc{Position{0, 0}, Position{0, 0}};
const Position kPos{0, 0};

inline AstNode* node(LuauAstNode* n) {
    return reinterpret_cast<AstNode*>(n);
}
inline AstExpr* expr(LuauAstNode* n) {
    return reinterpret_cast<AstExpr*>(node(n)->asExpr());
}
inline AstStat* stat(LuauAstNode* n) {
    return reinterpret_cast<AstStat*>(node(n)->asStat());
}
inline LuauAstNode* wrap(AstNode* n) {
    return reinterpret_cast<LuauAstNode*>(n);
}

// AstLocal and AstType are passed through the same opaque LuauAstNode* channel.
// AstLocal is NOT an AstNode, so it must never flow through node()/expr()/stat();
// it is reinterpret_cast directly to/from the opaque pointer.
inline LuauAstNode* wrapLocal(AstLocal* l) {
    return reinterpret_cast<LuauAstNode*>(l);
}
inline AstLocal* local(LuauAstNode* n) {
    return reinterpret_cast<AstLocal*>(n);
}
inline AstType* type(LuauAstNode* n) {
    return n ? reinterpret_cast<AstType*>(node(n)->asType()) : nullptr;
}

// Copy `n` opaque local pointers into an arena-allocated AstArray<AstLocal*>.
AstArray<AstLocal*> localArray(LuauAstBuilder* b, LuauAstNode** items, int n) {
    AstArray<AstLocal*> arr;
    arr.size = n < 0 ? 0 : static_cast<size_t>(n);
    arr.data = arr.size ? static_cast<AstLocal**>(b->allocator.allocate(sizeof(AstLocal*) * arr.size)) : nullptr;
    for (size_t i = 0; i < arr.size; i++)
        arr.data[i] = local(items[i]);
    return arr;
}

inline AstStatBlock* asBlock(LuauAstNode* n) {
    return reinterpret_cast<AstStatBlock*>(node(n)->asStat());
}

// Copy `n` opaque expr pointers into an arena-allocated AstArray<AstExpr*>.
AstArray<AstExpr*> exprArray(LuauAstBuilder* b, LuauAstNode** items, int n) {
    AstArray<AstExpr*> arr;
    arr.size = n < 0 ? 0 : static_cast<size_t>(n);
    arr.data = arr.size ? static_cast<AstExpr**>(b->allocator.allocate(sizeof(AstExpr*) * arr.size)) : nullptr;
    for (size_t i = 0; i < arr.size; i++)
        arr.data[i] = expr(items[i]);
    return arr;
}

// Copy `n` opaque stat pointers into an arena-allocated AstArray<AstStat*>.
AstArray<AstStat*> statArray(LuauAstBuilder* b, LuauAstNode** items, int n) {
    AstArray<AstStat*> arr;
    arr.size = n < 0 ? 0 : static_cast<size_t>(n);
    arr.data = arr.size ? static_cast<AstStat**>(b->allocator.allocate(sizeof(AstStat*) * arr.size)) : nullptr;
    for (size_t i = 0; i < arr.size; i++)
        arr.data[i] = stat(items[i]);
    return arr;
}

} // namespace

extern "C" LuauAstBuilder* luau_astbuild_new(void) {
    return new LuauAstBuilder();
}

extern "C" void luau_astbuild_free(LuauAstBuilder* b) {
    delete b;
}

extern "C" LuauAstNode* luau_astbuild_constant_nil(LuauAstBuilder* b) {
    return wrap(b->allocator.alloc<AstExprConstantNil>(kLoc));
}

extern "C" LuauAstNode* luau_astbuild_constant_bool(LuauAstBuilder* b, int value) {
    return wrap(b->allocator.alloc<AstExprConstantBool>(kLoc, value != 0));
}

extern "C" LuauAstNode* luau_astbuild_constant_number(LuauAstBuilder* b, double value) {
    return wrap(b->allocator.alloc<AstExprConstantNumber>(kLoc, value));
}

extern "C" LuauAstNode* luau_astbuild_constant_string(LuauAstBuilder* b, const char* s, size_t len) {
    AstArray<char> chars;
    chars.size = len;
    // Always hand back a non-null pointer (allocate len+1): the compiler's string
    // table asserts data != nullptr even for empty strings (Compiler.cpp sref()).
    chars.data = static_cast<char*>(b->allocator.allocate(len + 1));
    if (len)
        std::memcpy(chars.data, s, len);
    chars.data[len] = '\0';
    return wrap(b->allocator.alloc<AstExprConstantString>(kLoc, chars, AstExprConstantString::QuoteStyle::QuotedSimple));
}

extern "C" LuauAstNode* luau_astbuild_global(LuauAstBuilder* b, const char* name) {
    AstName n = b->names.getOrAdd(name);
    return wrap(b->allocator.alloc<AstExprGlobal>(kLoc, n));
}

extern "C" LuauAstNode* luau_astbuild_binary(LuauAstBuilder* b, int op, LuauAstNode* lhs, LuauAstNode* rhs) {
    return wrap(b->allocator.alloc<AstExprBinary>(
        kLoc, static_cast<AstExprBinary::Op>(op), expr(lhs), expr(rhs)));
}

extern "C" LuauAstNode* luau_astbuild_unary(LuauAstBuilder* b, int op, LuauAstNode* e) {
    return wrap(b->allocator.alloc<AstExprUnary>(
        kLoc, static_cast<AstExprUnary::Op>(op), expr(e)));
}

extern "C" LuauAstNode* luau_astbuild_call(LuauAstBuilder* b, LuauAstNode* func, LuauAstNode** args, int nargs) {
    AstArray<AstExpr*> a = exprArray(b, args, nargs);
    AstArray<AstTypeOrPack> noTypes;
    noTypes.data = nullptr;
    noTypes.size = 0;
    return wrap(b->allocator.alloc<AstExprCall>(
        kLoc, expr(func), a, /*self*/ false, noTypes, kLoc));
}

extern "C" LuauAstNode* luau_astbuild_index_name(LuauAstBuilder* b, LuauAstNode* e, const char* name) {
    AstName n = b->names.getOrAdd(name);
    return wrap(b->allocator.alloc<AstExprIndexName>(
        kLoc, expr(e), n, kLoc, kPos, '.'));
}

// `recv:name(args)` — a colon method call: the receiver binds once and is passed as the
// implicit self (AstExprCall.self = true). Reads idiomatically and avoids re-evaluating
// the receiver the way `recv.name(recv, args)` would.
extern "C" LuauAstNode* luau_astbuild_method_call(LuauAstBuilder* b, LuauAstNode* recv, const char* name, LuauAstNode** args, int nargs) {
    AstName n = b->names.getOrAdd(name);
    AstExpr* idx = b->allocator.alloc<AstExprIndexName>(kLoc, expr(recv), n, kLoc, kPos, ':');
    AstArray<AstExpr*> a = exprArray(b, args, nargs);
    AstArray<AstTypeOrPack> noTypes;
    noTypes.data = nullptr;
    noTypes.size = 0;
    return wrap(b->allocator.alloc<AstExprCall>(kLoc, idx, a, /*self*/ true, noTypes, kLoc));
}

extern "C" void luau_astbuild_set_location(LuauAstNode* n, int line, int col) {
    Position p{static_cast<unsigned>(line), static_cast<unsigned>(col)};
    reinterpret_cast<AstNode*>(n)->location = Location{p, p};
}

extern "C" LuauAstNode* luau_astbuild_group(LuauAstBuilder* b, LuauAstNode* e) {
    return wrap(b->allocator.alloc<AstExprGroup>(kLoc, expr(e)));
}

extern "C" LuauAstNode* luau_astbuild_return(LuauAstBuilder* b, LuauAstNode** exprs, int nexprs) {
    AstArray<AstExpr*> list = exprArray(b, exprs, nexprs);
    return wrap(b->allocator.alloc<AstStatReturn>(kLoc, list));
}

extern "C" LuauAstNode* luau_astbuild_expr_stat(LuauAstBuilder* b, LuauAstNode* e) {
    return wrap(b->allocator.alloc<AstStatExpr>(kLoc, expr(e)));
}

extern "C" LuauAstNode* luau_astbuild_block(LuauAstBuilder* b, LuauAstNode** stats, int nstats) {
    AstArray<AstStat*> body = statArray(b, stats, nstats);
    return wrap(b->allocator.alloc<AstStatBlock>(kLoc, body));
}

// --- locals & types --------------------------------------------------------

extern "C" LuauAstNode* luau_astbuild_local(LuauAstBuilder* b, const char* name, LuauAstNode* annotation) {
    AstName n = b->names.getOrAdd(name);
    AstLocal* l = b->allocator.alloc<AstLocal>(
        n, kLoc, /*shadow*/ nullptr, /*functionDepth*/ 0, /*loopDepth*/ 0, type(annotation));
    return wrapLocal(l);
}

extern "C" LuauAstNode* luau_astbuild_type_reference(LuauAstBuilder* b, const char* name) {
    AstName n = b->names.getOrAdd(name);
    AstArray<AstTypeOrPack> noParams;
    noParams.data = nullptr;
    noParams.size = 0;
    return wrap(b->allocator.alloc<AstTypeReference>(
        kLoc, std::nullopt, n, std::nullopt, kLoc, /*hasParameterList*/ false, noParams));
}

// --- expressions -----------------------------------------------------------

extern "C" LuauAstNode* luau_astbuild_constant_integer(LuauAstBuilder* b, long long value) {
    return wrap(b->allocator.alloc<AstExprConstantInteger>(kLoc, static_cast<int64_t>(value)));
}

extern "C" LuauAstNode* luau_astbuild_expr_local(LuauAstBuilder* b, LuauAstNode* l) {
    return wrap(b->allocator.alloc<AstExprLocal>(kLoc, local(l), /*upvalue*/ false));
}

extern "C" LuauAstNode* luau_astbuild_varargs(LuauAstBuilder* b) {
    return wrap(b->allocator.alloc<AstExprVarargs>(kLoc));
}

extern "C" LuauAstNode* luau_astbuild_index_expr(LuauAstBuilder* b, LuauAstNode* e, LuauAstNode* index) {
    return wrap(b->allocator.alloc<AstExprIndexExpr>(kLoc, expr(e), expr(index)));
}

extern "C" LuauAstNode* luau_astbuild_function(
    LuauAstBuilder* b, LuauAstNode** args, int nargs, int vararg, LuauAstNode** body, int nbody) {
    AstArray<AstLocal*> argArr = localArray(b, args, nargs);
    AstArray<AstStat*> bodyStats = statArray(b, body, nbody);
    AstStatBlock* block = b->allocator.alloc<AstStatBlock>(kLoc, bodyStats);

    AstArray<AstAttr*> noAttrs;
    noAttrs.data = nullptr;
    noAttrs.size = 0;
    AstArray<AstGenericType*> noGenerics;
    noGenerics.data = nullptr;
    noGenerics.size = 0;
    AstArray<AstGenericTypePack*> noGenericPacks;
    noGenericPacks.data = nullptr;
    noGenericPacks.size = 0;

    return wrap(b->allocator.alloc<AstExprFunction>(
        kLoc, noAttrs, noGenerics, noGenericPacks, /*self*/ nullptr, argArr,
        /*vararg*/ vararg != 0, kLoc, block, /*functionDepth*/ 0, AstName(),
        /*returnAnnotation*/ nullptr, /*varargAnnotation*/ nullptr, std::nullopt));
}

extern "C" LuauAstNode* luau_astbuild_table(
    LuauAstBuilder* b, int* kinds, LuauAstNode** keys, LuauAstNode** values, int nitems) {
    AstArray<AstExprTable::Item> items;
    items.size = nitems < 0 ? 0 : static_cast<size_t>(nitems);
    items.data = items.size
        ? static_cast<AstExprTable::Item*>(b->allocator.allocate(sizeof(AstExprTable::Item) * items.size))
        : nullptr;
    for (size_t i = 0; i < items.size; i++) {
        items.data[i].kind = static_cast<AstExprTable::Item::Kind>(kinds[i]);
        items.data[i].key = keys[i] ? expr(keys[i]) : nullptr;
        items.data[i].value = expr(values[i]);
    }
    return wrap(b->allocator.alloc<AstExprTable>(kLoc, items));
}

extern "C" LuauAstNode* luau_astbuild_type_assertion(LuauAstBuilder* b, LuauAstNode* e, LuauAstNode* annotation) {
    return wrap(b->allocator.alloc<AstExprTypeAssertion>(kLoc, expr(e), type(annotation)));
}

extern "C" LuauAstNode* luau_astbuild_if_else(
    LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* trueExpr, LuauAstNode* falseExpr) {
    return wrap(b->allocator.alloc<AstExprIfElse>(
        kLoc, expr(cond), /*hasThen*/ true, expr(trueExpr), /*hasElse*/ true, expr(falseExpr)));
}

extern "C" LuauAstNode* luau_astbuild_interp_string(
    LuauAstBuilder* b, const char** strings, size_t* lens, int nstrings, LuauAstNode** exprs, int nexprs) {
    AstArray<AstArray<char>> strs;
    strs.size = nstrings < 0 ? 0 : static_cast<size_t>(nstrings);
    strs.data = strs.size
        ? static_cast<AstArray<char>*>(b->allocator.allocate(sizeof(AstArray<char>) * strs.size))
        : nullptr;
    for (size_t i = 0; i < strs.size; i++) {
        size_t len = lens[i];
        strs.data[i].size = len;
        strs.data[i].data = len ? static_cast<char*>(b->allocator.allocate(len)) : nullptr;
        if (len)
            std::memcpy(strs.data[i].data, strings[i], len);
    }
    AstArray<AstExpr*> exprArr = exprArray(b, exprs, nexprs);
    return wrap(b->allocator.alloc<AstExprInterpString>(kLoc, strs, exprArr));
}

// --- statements ------------------------------------------------------------

extern "C" LuauAstNode* luau_astbuild_if(
    LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* thenBlock, LuauAstNode* elseStat) {
    return wrap(b->allocator.alloc<AstStatIf>(
        kLoc, expr(cond), asBlock(thenBlock), elseStat ? stat(elseStat) : nullptr, kLoc, std::nullopt));
}

extern "C" LuauAstNode* luau_astbuild_while(LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* body) {
    return wrap(b->allocator.alloc<AstStatWhile>(kLoc, expr(cond), asBlock(body), /*hasDo*/ true, kLoc));
}

extern "C" LuauAstNode* luau_astbuild_repeat(LuauAstBuilder* b, LuauAstNode* cond, LuauAstNode* body) {
    return wrap(b->allocator.alloc<AstStatRepeat>(kLoc, expr(cond), asBlock(body), /*hasUntil*/ true));
}

extern "C" LuauAstNode* luau_astbuild_break(LuauAstBuilder* b) {
    return wrap(b->allocator.alloc<AstStatBreak>(kLoc));
}

extern "C" LuauAstNode* luau_astbuild_continue(LuauAstBuilder* b) {
    return wrap(b->allocator.alloc<AstStatContinue>(kLoc));
}

extern "C" LuauAstNode* luau_astbuild_for(
    LuauAstBuilder* b, LuauAstNode* var, LuauAstNode* from, LuauAstNode* to, LuauAstNode* step, LuauAstNode* body) {
    return wrap(b->allocator.alloc<AstStatFor>(
        kLoc, local(var), expr(from), expr(to), step ? expr(step) : nullptr, asBlock(body), /*hasDo*/ true, kLoc));
}

extern "C" LuauAstNode* luau_astbuild_for_in(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues, LuauAstNode* body) {
    AstArray<AstLocal*> varArr = localArray(b, vars, nvars);
    AstArray<AstExpr*> valArr = exprArray(b, values, nvalues);
    return wrap(b->allocator.alloc<AstStatForIn>(
        kLoc, varArr, valArr, asBlock(body), /*hasIn*/ true, kLoc, /*hasDo*/ true, kLoc));
}

extern "C" LuauAstNode* luau_astbuild_assign(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues) {
    AstArray<AstExpr*> varArr = exprArray(b, vars, nvars);
    AstArray<AstExpr*> valArr = exprArray(b, values, nvalues);
    return wrap(b->allocator.alloc<AstStatAssign>(kLoc, varArr, valArr));
}

extern "C" LuauAstNode* luau_astbuild_compound_assign(LuauAstBuilder* b, int op, LuauAstNode* var, LuauAstNode* value) {
    return wrap(b->allocator.alloc<AstStatCompoundAssign>(
        kLoc, static_cast<AstExprBinary::Op>(op), expr(var), expr(value)));
}

extern "C" LuauAstNode* luau_astbuild_local_stat(
    LuauAstBuilder* b, LuauAstNode** vars, int nvars, LuauAstNode** values, int nvalues) {
    AstArray<AstLocal*> varArr = localArray(b, vars, nvars);
    AstArray<AstExpr*> valArr = exprArray(b, values, nvalues);
    return wrap(b->allocator.alloc<AstStatLocal>(
        kLoc, varArr, valArr, valArr.size ? std::optional<Location>(kLoc) : std::nullopt));
}

extern "C" LuauAstNode* luau_astbuild_function_stat(LuauAstBuilder* b, LuauAstNode* name, LuauAstNode* func) {
    AstExprFunction* f = reinterpret_cast<AstExprFunction*>(node(func)->asExpr());
    return wrap(b->allocator.alloc<AstStatFunction>(kLoc, expr(name), f));
}

extern "C" LuauAstNode* luau_astbuild_local_function(LuauAstBuilder* b, LuauAstNode* name, LuauAstNode* func) {
    AstExprFunction* f = reinterpret_cast<AstExprFunction*>(node(func)->asExpr());
    return wrap(b->allocator.alloc<AstStatLocalFunction>(kLoc, local(name), f));
}

// The builder allocates every AstLocal and AstExprFunction at functionDepth 0
// (it can't know nesting at construction time, since the AST is built bottom-up).
// The Luau compiler resolves upvalues by comparing a local's functionDepth to the
// enclosing function's depth, so leaving everything at 0 makes any closure that
// captures an outer local miscompile/crash. This pass walks the finished tree
// top-down and stamps the correct depth on every function and local declaration,
// which is exactly what the real parser does. Enables working closures/upvalues.
namespace {
struct DepthFixer : Luau::AstVisitor {
    size_t depth = 0;
    bool visit(Luau::AstStatLocal* s) override {
        for (Luau::AstLocal* l : s->vars)
            l->functionDepth = depth;
        return true;
    }
    bool visit(Luau::AstExprLocal* e) override {
        // a reference to a local declared in an enclosing function is an upvalue;
        // the compiler asserts this flag is set (Compiler.cpp LUAU_ASSERT(expr->upvalue))
        if (e->local)
            e->upvalue = e->local->functionDepth < depth;
        return true;
    }
    bool visit(Luau::AstStatFor* s) override {
        if (s->var)
            s->var->functionDepth = depth;
        return true;
    }
    bool visit(Luau::AstStatForIn* s) override {
        for (Luau::AstLocal* l : s->vars)
            l->functionDepth = depth;
        return true;
    }
    bool visit(Luau::AstStatLocalFunction* s) override {
        if (s->name)
            s->name->functionDepth = depth;
        return true;
    }
    bool visit(Luau::AstExprFunction* fn) override {
        // the root chunk is the implicit depth-0 "main" function, so a function
        // literal is one level deeper than its surrounding context.
        depth++;
        fn->functionDepth = depth;
        for (Luau::AstLocal* a : fn->args)
            a->functionDepth = depth;
        if (fn->body)
            fn->body->visit(this);
        depth--;
        return false; // descended manually with the adjusted depth
    }
};
} // namespace

// Luau's own pretty-printer is position-driven: it reproduces spacing by `advance()`ing
// to each node's source Location, and emits word operators (`or`/`and`) through an
// unguarded writer that glues them to a preceding identifier (`a or 5` -> `aor 5`, which
// is invalid). Our synthesized AST has no real positions, so rather than fabricate them
// we emit source ourselves. This small printer walks the exact node set the builder
// produces and is responsible for all spacing, so the output is always well-formed.
namespace {
// true if `s` is a valid Luau identifier and not a reserved keyword (so `.s` is legal).
static bool identSafe(const char* s) {
    if (!s || !*s) return false;
    if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    for (const char* p = s; *p; p++)
        if (!(std::isalnum((unsigned char)*p) || *p == '_')) return false;
    static const char* kw[] = {"and", "break", "do", "else", "elseif", "end", "false", "for", "function",
        "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"};
    for (const char* k : kw)
        if (std::strcmp(s, k) == 0) return false;
    return true;
}

struct SrcPrinter {
    std::string out;
    void pad(int ind) { out.append(size_t(ind) * 4, ' '); }

    void str(Luau::AstArray<char> s) {
        // single-quote unless the text contains one, then double-quote
        char q = '\'';
        for (size_t i = 0; i < s.size; i++)
            if (s.data[i] == '\'') { q = '"'; break; }
        out += q;
        for (size_t i = 0; i < s.size; i++) {
            char ch = s.data[i];
            switch (ch) {
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (ch == q) { out += '\\'; out += ch; }
                else if ((unsigned char)ch < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\%d", (unsigned char)ch); out += b; }
                else out += ch;
            }
        }
        out += q;
    }
    void num(double v) {
        if (std::isinf(v)) { out += v > 0 ? "1e500" : "-1e500"; return; }
        if (std::isnan(v)) { out += "0/0"; return; }
        if (v == (double)(long long)v && std::fabs(v) < 9e15) { char b[32]; std::snprintf(b, sizeof b, "%lld", (long long)v); out += b; }
        else { char b[40]; std::snprintf(b, sizeof b, "%.17g", v); out += b; }
    }

    // function tail shared by anonymous/local/named functions: `(args) <body> end`
    void funcTail(Luau::AstExprFunction* f, int ind, bool skipSelf = false) {
        out += '(';
        bool first = true;
        unsigned i = 0;
        for (Luau::AstLocal* a : f->args) {
            if (skipSelf && i++ == 0) continue; // `self` is implicit in a `t:m()` definition
            if (!first) out += ", "; first = false; out += a->name.value;
        }
        if (f->vararg) { if (!first) out += ", "; out += "..."; }
        out += ')';
        block(f->body, ind + 1);
        out += '\n'; pad(ind); out += "end";
    }

    void expr(Luau::AstExpr* e, int ind) {
        if (auto a = e->as<Luau::AstExprGroup>()) { out += '('; expr(a->expr, ind); out += ')'; }
        else if (e->is<Luau::AstExprConstantNil>()) out += "nil";
        else if (auto a = e->as<Luau::AstExprConstantBool>()) out += a->value ? "true" : "false";
        else if (auto a = e->as<Luau::AstExprConstantNumber>()) num(a->value);
        else if (auto a = e->as<Luau::AstExprConstantInteger>()) { char b[32]; std::snprintf(b, sizeof b, "%lld", (long long)a->value); out += b; }
        else if (auto a = e->as<Luau::AstExprConstantString>()) str(a->value);
        else if (auto a = e->as<Luau::AstExprLocal>()) out += a->local->name.value;
        else if (auto a = e->as<Luau::AstExprGlobal>()) out += a->name.value;
        else if (e->is<Luau::AstExprVarargs>()) out += "...";
        else if (auto a = e->as<Luau::AstExprCall>()) {
            expr(a->func, ind);
            out += '(';
            bool first = true;
            for (Luau::AstExpr* arg : a->args) { if (!first) out += ", "; first = false; expr(arg, ind); }
            out += ')';
        }
        else if (auto a = e->as<Luau::AstExprIndexName>()) {
            // `.name` is only valid for an identifier that isn't a reserved word;
            // otherwise (e.g. `.then`) fall back to bracket indexing: `["then"]`.
            if (a->op == '.' && !identSafe(a->index.value)) { expr(a->expr, ind); out += "[\""; out += a->index.value; out += "\"]"; }
            else { expr(a->expr, ind); out += a->op; out += a->index.value; }
        }
        else if (auto a = e->as<Luau::AstExprIndexExpr>()) { expr(a->expr, ind); out += '['; expr(a->index, ind); out += ']'; }
        else if (auto a = e->as<Luau::AstExprFunction>()) { out += "function"; funcTail(a, ind); }
        else if (auto a = e->as<Luau::AstExprTable>()) {
            out += '{';
            bool first = true;
            for (const auto& it : a->items) {
                if (!first) out += ", "; first = false;
                if (it.kind == Luau::AstExprTable::Item::Kind::Record) { out += it.key->as<Luau::AstExprConstantString>()->value.data; out += " = "; }
                else if (it.kind == Luau::AstExprTable::Item::Kind::General) { out += '['; expr(it.key, ind); out += "] = "; }
                expr(it.value, ind);
            }
            out += '}';
        }
        else if (auto a = e->as<Luau::AstExprUnary>()) {
            switch (a->op) {
            case Luau::AstExprUnary::Op::Not: out += "not "; break;
            case Luau::AstExprUnary::Op::Minus: out += '-'; break;
            case Luau::AstExprUnary::Op::Len: out += '#'; break;
            }
            // unary binds tighter than binary/if-else, so `not (a > b)` needs the parens
            // (without them `not a > b` would parse as `(not a) > b`).
            if (a->expr->is<Luau::AstExprBinary>() || a->expr->is<Luau::AstExprIfElse>()) { out += '('; expr(a->expr, ind); out += ')'; }
            else expr(a->expr, ind);
        }
        else if (auto a = e->as<Luau::AstExprBinary>()) { expr(a->left, ind); out += ' '; out += Luau::toString(a->op); out += ' '; expr(a->right, ind); }
        else if (auto a = e->as<Luau::AstExprIfElse>()) {
            out += "if "; expr(a->condition, ind); out += " then "; expr(a->trueExpr, ind);
            out += " else "; expr(a->falseExpr, ind);
        }
        else if (auto a = e->as<Luau::AstExprTypeAssertion>()) expr(a->expr, ind); // drop the cast; runtime is unaffected
        else if (auto a = e->as<Luau::AstExprInterpString>()) {
            out += '`';
            for (size_t i = 0; i < a->strings.size; i++) {
                Luau::AstArray<char> chunk = a->strings.data[i];
                out.append(chunk.data, chunk.size);
                if (i < a->expressions.size) { out += '{'; expr(a->expressions.data[i], ind); out += '}'; }
            }
            out += '`';
        }
        else out += "--[[?]]"; // unreachable for the builder's node set
    }

    void names(Luau::AstArray<Luau::AstLocal*> vars) {
        bool first = true;
        for (Luau::AstLocal* v : vars) { if (!first) out += ", "; first = false; out += v->name.value; }
    }
    void exprs(Luau::AstArray<Luau::AstExpr*> es, int ind) {
        bool first = true;
        for (Luau::AstExpr* e : es) { if (!first) out += ", "; first = false; expr(e, ind); }
    }

    void stat(Luau::AstStat* s, int ind) {
        if (auto a = s->as<Luau::AstStatBlock>()) { out += "do"; block(a, ind + 1); out += '\n'; pad(ind); out += "end"; }
        else if (auto a = s->as<Luau::AstStatLocal>()) {
            out += "local "; names(a->vars);
            if (a->values.size) { out += " = "; exprs(a->values, ind); }
        }
        else if (auto a = s->as<Luau::AstStatLocalFunction>()) { out += "local function "; out += a->name->name.value; funcTail(a->func, ind); }
        else if (auto a = s->as<Luau::AstStatFunction>()) {
            // a method `function t.m(self, …)` prints idiomatically as `function t:m(…)`
            auto idx = a->name->as<Luau::AstExprIndexName>();
            bool method = idx && idx->op == '.' && a->func->args.size >= 1 && std::strcmp(a->func->args.data[0]->name.value, "self") == 0;
            if (method) { out += "function "; expr(idx->expr, ind); out += ':'; out += idx->index.value; funcTail(a->func, ind, true); }
            else { out += "function "; expr(a->name, ind); funcTail(a->func, ind); }
        }
        else if (auto a = s->as<Luau::AstStatAssign>()) { exprs(a->vars, ind); out += " = "; exprs(a->values, ind); }
        else if (auto a = s->as<Luau::AstStatCompoundAssign>()) { expr(a->var, ind); out += ' '; out += Luau::toString(a->op); out += "= "; expr(a->value, ind); }
        else if (auto a = s->as<Luau::AstStatExpr>()) expr(a->expr, ind);
        else if (auto a = s->as<Luau::AstStatReturn>()) { out += "return"; if (a->list.size) { out += ' '; exprs(a->list, ind); } }
        else if (s->is<Luau::AstStatBreak>()) out += "break";
        else if (s->is<Luau::AstStatContinue>()) out += "continue";
        else if (auto a = s->as<Luau::AstStatWhile>()) { out += "while "; expr(a->condition, ind); out += " do"; block(a->body, ind + 1); out += '\n'; pad(ind); out += "end"; }
        else if (auto a = s->as<Luau::AstStatRepeat>()) { out += "repeat"; block(a->body, ind + 1); out += '\n'; pad(ind); out += "until "; expr(a->condition, ind); }
        else if (auto a = s->as<Luau::AstStatFor>()) {
            out += "for "; out += a->var->name.value; out += " = "; expr(a->from, ind); out += ", "; expr(a->to, ind);
            if (a->step) { out += ", "; expr(a->step, ind); }
            out += " do"; block(a->body, ind + 1); out += '\n'; pad(ind); out += "end";
        }
        else if (auto a = s->as<Luau::AstStatForIn>()) {
            out += "for "; names(a->vars); out += " in "; exprs(a->values, ind);
            out += " do"; block(a->body, ind + 1); out += '\n'; pad(ind); out += "end";
        }
        else if (auto a = s->as<Luau::AstStatIf>()) {
            out += "if "; expr(a->condition, ind); out += " then"; block(a->thenbody, ind + 1);
            for (Luau::AstStat* el = a->elsebody; el;) {
                if (auto chain = el->as<Luau::AstStatIf>()) { // `else if` -> `elseif`
                    out += '\n'; pad(ind); out += "elseif "; expr(chain->condition, ind); out += " then"; block(chain->thenbody, ind + 1);
                    el = chain->elsebody;
                } else { // plain else block
                    out += '\n'; pad(ind); out += "else"; block(el->as<Luau::AstStatBlock>(), ind + 1);
                    el = nullptr;
                }
            }
            out += '\n'; pad(ind); out += "end";
        }
        else out += "--[[?]]";
    }

    // Render a single statement to its own string (no leading newline/indent). Guards the
    // Lua call ambiguity: a non-first statement that renders starting with `(` would bind
    // as a call to the previous statement's trailing value (`x = f()` <newline> `(g)()`),
    // so it gets a leading `;`. A first statement can't be ambiguous (it follows
    // `do`/`then`/`=`, not a value).
    std::string render(Luau::AstStat* s, int ind, bool first) {
        std::string saved;
        std::swap(saved, out);
        stat(s, ind);
        std::string r;
        std::swap(r, out);
        std::swap(saved, out);
        if (!first && !r.empty() && r[0] == '(') r.insert(r.begin(), ';');
        return r;
    }

    // a function literal or an IIFE wrapping one (`(function() … end)()`, e.g. a class body)
    static bool isFuncValue(Luau::AstExpr* e) {
        if (e->is<Luau::AstExprFunction>()) return true;
        if (auto c = e->as<Luau::AstExprCall>()) {
            Luau::AstExpr* f = c->func;
            if (auto g = f->as<Luau::AstExprGroup>()) return g->expr->is<Luau::AstExprFunction>();
            return f->is<Luau::AstExprFunction>();
        }
        return false;
    }
    // a "definition" statement: a named/local function, or a binding to a function/class.
    static bool isDef(Luau::AstStat* s) {
        if (s->is<Luau::AstStatFunction>() || s->is<Luau::AstStatLocalFunction>()) return true;
        if (auto a = s->as<Luau::AstStatLocal>()) return a->values.size == 1 && isFuncValue(a->values.data[0]);
        if (auto a = s->as<Luau::AstStatAssign>()) return a->values.size == 1 && isFuncValue(a->values.data[0]);
        return false;
    }

    // Emit a sequence of statements, one per line. A blank line is inserted between two
    // statements when either is a definition (function/class), so definitions stand apart
    // while runs of ordinary statements stay grouped. `topLevel` suppresses the leading
    // newline before the very first statement.
    void emitStatements(Luau::AstArray<Luau::AstStat*> body, int ind, bool topLevel) {
        for (size_t i = 0; i < body.size; i++) {
            std::string r = render(body.data[i], ind, i == 0);
            if (i || !topLevel) out += '\n';
            if (i && (isDef(body.data[i]) || isDef(body.data[i - 1]))) out += '\n';
            pad(ind);
            out += r;
        }
    }
    void block(Luau::AstStatBlock* blk, int ind) { emitStatements(blk->body, ind, false); }
};
} // namespace

// Pretty-print a built AST back to clean, well-formed Luau source. Returns a malloc'd C string.
extern "C" char* luau_astbuild_prettyprint(LuauAstBuilder* b, LuauAstNode* rootBlock) {
    (void)b;
    try {
        AstStatBlock* root = reinterpret_cast<AstStatBlock*>(node(rootBlock)->asStat());
        SrcPrinter p;
        p.emitStatements(root->body, 0, /*topLevel*/ true);
        char* buf = static_cast<char*>(std::malloc(p.out.size() + 1));
        std::memcpy(buf, p.out.data(), p.out.size());
        buf[p.out.size()] = '\0';
        return buf;
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" char* luau_astbuild_compile(LuauAstBuilder* b, LuauAstNode* rootBlock, int* out_len, char** out_err) {
    if (out_len)
        *out_len = 0;
    if (out_err)
        *out_err = nullptr;
    try {
        ParseResult parseResult;
        parseResult.root = reinterpret_cast<AstStatBlock*>(node(rootBlock)->asStat());

        // assign correct nesting depths so closures resolve upvalues
        DepthFixer depthFixer;
        parseResult.root->visit(&depthFixer);

        BytecodeBuilder bcb;
        // compileOrThrow already finalizes the bytecode internally; calling
        // finalize() again would trip LUAU_ASSERT(bytecode.empty()).
        compileOrThrow(bcb, parseResult, b->names);

        const std::string& bc = bcb.getBytecode();
        char* buf = static_cast<char*>(std::malloc(bc.size() ? bc.size() : 1));
        if (bc.size())
            std::memcpy(buf, bc.data(), bc.size());
        if (out_len)
            *out_len = static_cast<int>(bc.size());
        return buf;
    } catch (const std::exception& e) {
        if (out_err) {
            const char* msg = e.what();
            size_t n = std::strlen(msg);
            char* m = static_cast<char*>(std::malloc(n + 1));
            std::memcpy(m, msg, n + 1);
            *out_err = m;
        }
        return nullptr;
    }
}
