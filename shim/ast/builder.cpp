// extern "C" shim over Luau AST construction + compilation (Ast + Compiler).

#include "builder.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Lexer.h"
#include "Luau/Location.h"
#include "Luau/ParseResult.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

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
    chars.data = len ? static_cast<char*>(b->allocator.allocate(len)) : nullptr;
    if (len)
        std::memcpy(chars.data, s, len);
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

extern "C" char* luau_astbuild_compile(LuauAstBuilder* b, LuauAstNode* rootBlock, int* out_len, char** out_err) {
    if (out_len)
        *out_len = 0;
    if (out_err)
        *out_err = nullptr;
    try {
        ParseResult parseResult;
        parseResult.root = reinterpret_cast<AstStatBlock*>(node(rootBlock)->asStat());

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
