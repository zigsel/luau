//! Idiomatic wrapper over the Luau documentation database (via the C++ shim).
//!
//! LIMIT — Luau's public `Documentation.h` ships only the in-memory data model
//! (the `BasicDocumentation`/`FunctionDocumentation`/... structs and the
//! `DenseHashMap<symbol, Documentation>` alias); there is NO bundled JSON loader
//! or file parser, and no public "look up a symbol" helper. An editor builds the
//! database itself and indexes it by the `documentationSymbol` string carried on
//! autocomplete entries.
//!
//! This wrapper therefore exposes the bridge that does exist: an in-memory
//! database you populate with `BasicDocumentation` entries and then query by
//! symbol — mapping an autocomplete `documentationSymbol` to its hover text,
//! code sample and learn-more link. Only the BasicDocumentation variant is
//! surfaced (the rest reference other symbols rather than carrying text).

const std = @import("std");
const c = @import("bindings");

/// The documentation text, code sample and learn-more link for a symbol. All
/// slices borrow the owning `Database` and are valid until its next mutation or
/// `deinit`.
pub const Entry = struct {
    text: []const u8,
    code_sample: []const u8,
    learn_more: []const u8,
};

/// An in-memory documentation database. Owns the underlying handle; call
/// `deinit` when done.
pub const Database = struct {
    handle: *c.LuauDocs,

    pub fn init() error{OutOfMemory}!Database {
        const handle = c.luau_docs_new() orelse return error.OutOfMemory;
        return .{ .handle = handle };
    }

    pub fn deinit(self: Database) void {
        c.luau_docs_free(self.handle);
    }

    /// Insert (or overwrite) a documentation entry keyed by `symbol`. Pass empty
    /// strings for fields you don't have.
    pub fn addBasic(
        self: Database,
        symbol: [:0]const u8,
        documentation: [:0]const u8,
        learn_more: [:0]const u8,
        code_sample: [:0]const u8,
    ) error{AddFailed}!void {
        if (c.luau_docs_add_basic(self.handle, symbol.ptr, documentation.ptr, learn_more.ptr, code_sample.ptr) == 0)
            return error.AddFailed;
    }

    /// Number of entries.
    pub fn count(self: Database) usize {
        return @intCast(c.luau_docs_count(self.handle));
    }

    /// Whether `symbol` resolves to a BasicDocumentation entry.
    pub fn has(self: Database, symbol: [:0]const u8) bool {
        return c.luau_docs_has(self.handle, symbol.ptr) != 0;
    }

    /// Look up the full entry for `symbol`, or null if absent.
    pub fn lookup(self: Database, symbol: [:0]const u8) ?Entry {
        const text = c.luau_docs_text(self.handle, symbol.ptr) orelse return null;
        return .{
            .text = std.mem.span(text),
            .code_sample = spanOrEmpty(c.luau_docs_code_sample(self.handle, symbol.ptr)),
            .learn_more = spanOrEmpty(c.luau_docs_learn_more(self.handle, symbol.ptr)),
        };
    }
};

fn spanOrEmpty(cstr: [*c]const u8) []const u8 {
    return if (cstr) |p| std.mem.span(p) else "";
}
