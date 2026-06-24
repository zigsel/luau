//! Idiomatic wrapper over the Luau Ast lexer (via the C++ shim).

const std = @import("std");
const c = @import("bindings");

/// A 0-based source position (mirrors `ast.Position`).
pub const Position = struct {
    line: u32,
    column: u32,
};

/// A lexeme kind. Values match `Luau::Lexeme::Type`; `Eof` is 0 and 1..255 are
/// literal character byte values. The named variants cover the multi-character
/// and reserved-word tokens.
pub const Kind = enum(c_int) {
    eof = 0,

    equal = 257,
    less_equal,
    greater_equal,
    not_equal,
    dot2,
    dot3,
    skinny_arrow,
    double_colon,
    floor_div,

    interp_string_begin,
    interp_string_mid,
    interp_string_end,
    interp_string_simple,

    add_assign,
    sub_assign,
    mul_assign,
    div_assign,
    floor_div_assign,
    mod_assign,
    pow_assign,
    concat_assign,

    raw_string,
    quoted_string,
    number,
    name,

    comment,
    block_comment,

    attribute,
    attribute_open,

    broken_string,
    broken_comment,
    broken_unicode,
    broken_interp_double_brace,
    @"error",

    reserved_and,
    reserved_break,
    reserved_do,
    reserved_else,
    reserved_elseif,
    reserved_end,
    reserved_false,
    reserved_for,
    reserved_function,
    reserved_if,
    reserved_in,
    reserved_local,
    reserved_nil,
    reserved_not,
    reserved_or,
    reserved_repeat,
    reserved_return,
    reserved_then,
    reserved_true,
    reserved_until,
    reserved_while,
    _,
};

/// A single token produced by the lexer.
pub const Token = struct {
    kind: Kind,
    begin: Position,
    end: Position,
    /// The token's text, where meaningful (names/strings/numbers/comments);
    /// "" otherwise. Borrows the owning `Tokens` storage.
    text: []const u8,
};

/// A tokenized source string. Owns the underlying token list; call `deinit`.
/// Token text borrows this storage and is valid until then.
pub const Tokens = struct {
    handle: *c.LuauTokens,

    pub fn deinit(self: Tokens) void {
        c.luau_ast_tokens_free(self.handle);
    }

    /// Number of tokens (excludes the terminal Eof).
    pub fn count(self: Tokens) usize {
        return @intCast(c.luau_ast_tokens_count(self.handle));
    }

    /// The `i`-th token.
    pub fn get(self: Tokens, i: usize) Token {
        const begin = c.luau_ast_token_begin(self.handle, @intCast(i));
        const end = c.luau_ast_token_end(self.handle, @intCast(i));
        return .{
            .kind = @enumFromInt(c.luau_ast_token_type(self.handle, @intCast(i))),
            .begin = .{ .line = begin.line, .column = begin.column },
            .end = .{ .line = end.line, .column = end.column },
            .text = std.mem.span(c.luau_ast_token_text(self.handle, @intCast(i))),
        };
    }

    /// Iterate the tokens in source order.
    pub fn iterator(self: Tokens) Iterator {
        return .{ .tokens = self, .i = 0, .n = self.count() };
    }
};

pub const Iterator = struct {
    tokens: Tokens,
    i: usize,
    n: usize,

    pub fn next(self: *Iterator) ?Token {
        if (self.i >= self.n) return null;
        defer self.i += 1;
        return self.tokens.get(self.i);
    }
};

/// Tokenize `src`. Always succeeds; lexical errors surface as `.@"error"` or
/// `.broken_*` tokens.
pub fn lex(src: []const u8) Tokens {
    return .{ .handle = c.luau_ast_lex(src.ptr, src.len).? };
}
