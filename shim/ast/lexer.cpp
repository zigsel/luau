// extern "C" shim over Luau::Lexer (Ast module).

#include "lexer.h"

#include "Luau/Allocator.h"
#include "Luau/Lexer.h"

#include <string>
#include <vector>

using namespace Luau;

namespace {

// One captured token: type, span, and (where meaningful) its text.
struct Token {
    int type;
    LuauPosition begin;
    LuauPosition end;
    std::string text;
};

// Extract the meaningful text for a lexeme, where it carries one.
static std::string tokenText(const Lexeme& lex) {
    switch (lex.type) {
    case Lexeme::Name:
        return lex.name ? std::string(lex.name) : std::string();
    case Lexeme::RawString:
    case Lexeme::QuotedString:
    case Lexeme::InterpStringBegin:
    case Lexeme::InterpStringMid:
    case Lexeme::InterpStringEnd:
    case Lexeme::InterpStringSimple:
    case Lexeme::Number:
    case Lexeme::Comment:
    case Lexeme::BlockComment:
    case Lexeme::BrokenComment:
    case Lexeme::BrokenString:
        // These store a slice into the input buffer: pointer + length.
        if (lex.data)
            return std::string(lex.data, lex.getLength());
        return std::string();
    default:
        return std::string();
    }
}

} // namespace

struct LuauTokens {
    Allocator allocator;
    AstNameTable names;
    std::vector<Token> tokens;

    LuauTokens()
        : allocator()
        , names(allocator)
    {
    }
};

extern "C" LuauTokens* luau_ast_lex(const char* src, size_t len) {
    LuauTokens* h = new LuauTokens();
    try {
        Lexer lexer(src, len, h->names);
        lexer.setSkipComments(false);
        lexer.setReadNames(true);

        for (;;) {
            const Lexeme& lex = lexer.next();
            if (lex.type == Lexeme::Eof)
                break;

            Token t;
            t.type = static_cast<int>(lex.type);
            t.begin = LuauPosition{lex.location.begin.line, lex.location.begin.column};
            t.end = LuauPosition{lex.location.end.line, lex.location.end.column};
            t.text = tokenText(lex);
            h->tokens.push_back(std::move(t));

            // Defensive: lexers emit Eof at end; Error tokens should not loop.
            if (lex.type == Lexeme::Error)
                break;
        }
    } catch (...) {
        // Return whatever was captured so far.
    }
    return h;
}

extern "C" int luau_ast_tokens_count(const LuauTokens* h) {
    return static_cast<int>(h->tokens.size());
}

extern "C" int luau_ast_token_type(const LuauTokens* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->tokens.size())
        return 0;
    return h->tokens[i].type;
}

extern "C" LuauPosition luau_ast_token_begin(const LuauTokens* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->tokens.size())
        return LuauPosition{0, 0};
    return h->tokens[i].begin;
}

extern "C" LuauPosition luau_ast_token_end(const LuauTokens* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->tokens.size())
        return LuauPosition{0, 0};
    return h->tokens[i].end;
}

extern "C" const char* luau_ast_token_text(const LuauTokens* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->tokens.size())
        return "";
    return h->tokens[i].text.c_str();
}

extern "C" void luau_ast_tokens_free(LuauTokens* h) {
    delete h;
}
