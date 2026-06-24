//! Idiomatic wrapper over Luau's richer documentation variants (via the C++
//! shim) — the data behind signature help and structured hover.
//!
//! The sibling `docs` module surfaces only `BasicDocumentation` (flat hover
//! text). This module surfaces the three richer `Documentation.h` variants:
//!
//!   * `Function`   — a callable: doc text plus an ordered list of parameters
//!                    (each with a name and its own documentation symbol) and a
//!                    list of return documentation symbols. This is exactly the
//!                    param-name / per-parameter-doc data signature help renders.
//!   * `Table`      — a table/class: doc text plus a map from key name to that
//!                    key's documentation symbol.
//!   * `Overloaded` — a map from a function-signature string to that overload's
//!                    documentation symbol.
//!
//! LIMIT — as with `docs`, Luau ships these structs but NO loader/parser: an
//! editor builds the database itself and indexes it by the `documentationSymbol`
//! an autocomplete entry carries. This wrapper is therefore an in-memory builder
//! + reader. Pure data; no solver, no Frontend.
//!
//! All returned slices borrow the owning `Database` and are valid until its next
//! mutation or `deinit`.

const std = @import("std");
const c = @import("bindings");

fn spanOrNull(cstr: [*c]const u8) ?[]const u8 {
    return if (cstr) |p| std.mem.span(p) else null;
}

/// One function parameter's documentation: its name, and the documentation
/// symbol describing the parameter (look the symbol up in a `docs.Database`).
pub const Parameter = struct {
    name: []const u8,
    doc_symbol: []const u8,
};

/// An in-memory signature/documentation database. Owns the underlying handle;
/// call `deinit` when done.
pub const Database = struct {
    handle: *c.LuauDocs,

    pub fn init() error{OutOfMemory}!Database {
        const handle = c.luau_signatures_new() orelse return error.OutOfMemory;
        return .{ .handle = handle };
    }

    pub fn deinit(self: Database) void {
        c.luau_signatures_free(self.handle);
    }

    /// Total number of entries across all variants.
    pub fn count(self: Database) usize {
        return @intCast(c.luau_signatures_count(self.handle));
    }

    // ----- functions ------------------------------------------------------

    /// Insert (or overwrite) a function entry keyed by `symbol`. It starts with
    /// no parameters/returns; add them with `addParameter` / `addReturn`. Pass
    /// empty strings for fields you don't have.
    pub fn addFunction(
        self: Database,
        symbol: [:0]const u8,
        documentation: [:0]const u8,
        learn_more: [:0]const u8,
        code_sample: [:0]const u8,
    ) error{AddFailed}!void {
        if (c.luau_signatures_add_function(self.handle, symbol.ptr, documentation.ptr, learn_more.ptr, code_sample.ptr) == 0)
            return error.AddFailed;
    }

    /// Append a parameter (name + its documentation symbol) to function
    /// `symbol`. `doc_symbol` may be empty.
    pub fn addParameter(self: Database, symbol: [:0]const u8, name: [:0]const u8, doc_symbol: [:0]const u8) error{AddFailed}!void {
        if (c.luau_signatures_function_add_parameter(self.handle, symbol.ptr, name.ptr, doc_symbol.ptr) == 0)
            return error.AddFailed;
    }

    /// Append a return documentation symbol to function `symbol`.
    pub fn addReturn(self: Database, symbol: [:0]const u8, doc_symbol: [:0]const u8) error{AddFailed}!void {
        if (c.luau_signatures_function_add_return(self.handle, symbol.ptr, doc_symbol.ptr) == 0)
            return error.AddFailed;
    }

    /// Whether `symbol` is a function entry.
    pub fn isFunction(self: Database, symbol: [:0]const u8) bool {
        return c.luau_signatures_is_function(self.handle, symbol.ptr) != 0;
    }

    /// The function's own doc text, or null if `symbol` is not a function entry.
    pub fn functionDocumentation(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_function_documentation(self.handle, symbol.ptr));
    }
    /// The function's learn-more link, or null if not a function entry.
    pub fn functionLearnMore(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_function_learn_more(self.handle, symbol.ptr));
    }
    /// The function's code sample, or null if not a function entry.
    pub fn functionCodeSample(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_function_code_sample(self.handle, symbol.ptr));
    }

    /// Number of parameters recorded for function `symbol` (0 if not a function).
    pub fn parameterCount(self: Database, symbol: [:0]const u8) usize {
        return @intCast(c.luau_signatures_function_parameter_count(self.handle, symbol.ptr));
    }
    /// Number of return documentation symbols recorded for function `symbol`.
    pub fn returnCount(self: Database, symbol: [:0]const u8) usize {
        return @intCast(c.luau_signatures_function_return_count(self.handle, symbol.ptr));
    }

    /// Parameter `i` of function `symbol`, or null if out of range.
    pub fn parameter(self: Database, symbol: [:0]const u8, i: usize) ?Parameter {
        const name = c.luau_signatures_function_parameter_name(self.handle, symbol.ptr, @intCast(i)) orelse return null;
        return .{
            .name = std.mem.span(name),
            .doc_symbol = spanOrNull(c.luau_signatures_function_parameter_doc(self.handle, symbol.ptr, @intCast(i))) orelse "",
        };
    }

    /// Documentation symbol of return `i` of function `symbol`, or null if out
    /// of range.
    pub fn returnSymbol(self: Database, symbol: [:0]const u8, i: usize) ?[]const u8 {
        return spanOrNull(c.luau_signatures_function_return(self.handle, symbol.ptr, @intCast(i)));
    }

    /// Iterate the parameters of function `symbol`.
    pub fn parameters(self: Database, symbol: [:0]const u8) ParameterIterator {
        return .{ .db = self, .symbol = symbol, .n = self.parameterCount(symbol) };
    }
    pub const ParameterIterator = struct {
        db: Database,
        symbol: [:0]const u8,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ParameterIterator) ?Parameter {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.db.parameter(it.symbol, it.i);
        }
    };

    // ----- tables ---------------------------------------------------------

    /// Insert (or overwrite) a table/class entry keyed by `symbol`. Starts with
    /// no keys; add them with `addKey`.
    pub fn addTable(
        self: Database,
        symbol: [:0]const u8,
        documentation: [:0]const u8,
        learn_more: [:0]const u8,
        code_sample: [:0]const u8,
    ) error{AddFailed}!void {
        if (c.luau_signatures_add_table(self.handle, symbol.ptr, documentation.ptr, learn_more.ptr, code_sample.ptr) == 0)
            return error.AddFailed;
    }

    /// Map table key `key` to its documentation symbol on table `symbol`.
    pub fn addKey(self: Database, symbol: [:0]const u8, key: [:0]const u8, key_doc_symbol: [:0]const u8) error{AddFailed}!void {
        if (c.luau_signatures_table_add_key(self.handle, symbol.ptr, key.ptr, key_doc_symbol.ptr) == 0)
            return error.AddFailed;
    }

    /// Whether `symbol` is a table entry.
    pub fn isTable(self: Database, symbol: [:0]const u8) bool {
        return c.luau_signatures_is_table(self.handle, symbol.ptr) != 0;
    }

    /// The table's own doc text, or null if not a table entry.
    pub fn tableDocumentation(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_table_documentation(self.handle, symbol.ptr));
    }
    /// The table's learn-more link, or null if not a table entry.
    pub fn tableLearnMore(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_table_learn_more(self.handle, symbol.ptr));
    }
    /// The table's code sample, or null if not a table entry.
    pub fn tableCodeSample(self: Database, symbol: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_table_code_sample(self.handle, symbol.ptr));
    }

    /// Documentation symbol for table key `key`, or null if absent.
    pub fn keyDoc(self: Database, symbol: [:0]const u8, key: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_table_key_doc(self.handle, symbol.ptr, key.ptr));
    }

    // ----- overloaded functions -------------------------------------------

    /// Insert (or overwrite) an empty overloaded-function entry keyed by
    /// `symbol`. Add overloads with `addOverload`.
    pub fn addOverloaded(self: Database, symbol: [:0]const u8) error{AddFailed}!void {
        if (c.luau_signatures_add_overloaded(self.handle, symbol.ptr) == 0)
            return error.AddFailed;
    }

    /// Map overload `signature` to its documentation symbol on `symbol`.
    pub fn addOverload(self: Database, symbol: [:0]const u8, signature: [:0]const u8, overload_doc_symbol: [:0]const u8) error{AddFailed}!void {
        if (c.luau_signatures_overloaded_add(self.handle, symbol.ptr, signature.ptr, overload_doc_symbol.ptr) == 0)
            return error.AddFailed;
    }

    /// Whether `symbol` is an overloaded-function entry.
    pub fn isOverloaded(self: Database, symbol: [:0]const u8) bool {
        return c.luau_signatures_is_overloaded(self.handle, symbol.ptr) != 0;
    }

    /// Documentation symbol for overload `signature`, or null if absent.
    pub fn overloadDoc(self: Database, symbol: [:0]const u8, signature: [:0]const u8) ?[]const u8 {
        return spanOrNull(c.luau_signatures_overloaded_doc(self.handle, symbol.ptr, signature.ptr));
    }
};
