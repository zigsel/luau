// Shim: Luau Ast lexer (tokenize source into a token list).
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Tokenize `src` (length `len`) into an owned token list. Always returns a
// handle (free with luau_ast_tokens_free); a lexical error surfaces as an
// Error/Broken* token rather than a failure to return.
LuauTokens* luau_ast_lex(const char* src, size_t len);

// Number of tokens (excludes the terminal Eof).
int luau_ast_tokens_count(const LuauTokens* h);

// The `i`-th token's type (a Luau::Lexeme::Type value), or 0 (Eof) if out of range.
int luau_ast_token_type(const LuauTokens* h, int i);

// The `i`-th token's source span.
LuauPosition luau_ast_token_begin(const LuauTokens* h, int i);
LuauPosition luau_ast_token_end(const LuauTokens* h, int i);

// The `i`-th token's text as a NUL-terminated string, owned by the handle.
// Meaningful for names/strings/numbers/comments; "" otherwise.
const char* luau_ast_token_text(const LuauTokens* h, int i);

void luau_ast_tokens_free(LuauTokens* h);

LUAU_END_DECLS
