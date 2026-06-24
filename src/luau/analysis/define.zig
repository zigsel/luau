//! Idiomatic wrapper over go-to-definition and top-level symbol locations,
//! via the C++ Analysis shim.

const std = @import("std");
const c = @import("bindings");

/// A 0-based source position.
pub const Position = struct {
    line: u32,
    column: u32,
};

/// A declaration span: where a symbol is declared.
pub const Span = struct {
    begin: Position,
    end: Position,
};

/// Resolve the DECLARATION span of the symbol at (`line`, `col`) (0-based)
/// within the self-contained Luau module `src`.
///
/// Returns the declaration span, or `null` if nothing could be resolved at
/// that position.
pub fn definition(src: []const u8, line: u32, col: u32) ?Span {
    var begin: c.LuauPosition = undefined;
    var end: c.LuauPosition = undefined;
    const ok = c.luau_analysis_definition(
        src.ptr,
        src.len,
        @intCast(line),
        @intCast(col),
        &begin,
        &end,
    );
    if (ok == 0) return null;
    return .{
        .begin = .{ .line = begin.line, .column = begin.column },
        .end = .{ .line = end.line, .column = end.column },
    };
}

/// A single top-level binding declaration (fields borrow the result's storage,
/// except `name` which is owned and must be freed via `Symbols.freeName`).
pub const Symbol = struct {
    name: [*c]u8,
    span: Span,

    /// The borrowed name as a slice. Valid until `Symbols.freeName` is called.
    pub fn nameSlice(self: Symbol) [:0]const u8 {
        return std.mem.span(self.name);
    }

    /// Free the malloc'd name string.
    pub fn freeName(self: Symbol) void {
        std.c.free(self.name);
    }
};

/// All top-level binding declarations of a module. Caller owns it and must
/// call `deinit`.
pub const Symbols = struct {
    handle: ?*c.LuauSymbols,

    pub fn deinit(self: Symbols) void {
        c.luau_analysis_symbols_free(self.handle);
    }

    pub fn count(self: Symbols) usize {
        return @intCast(c.luau_analysis_symbols_count(self.handle));
    }

    /// The `i`-th symbol. The returned `name` is malloc'd; free it with
    /// `Symbol.freeName`.
    pub fn at(self: Symbols, i: usize) Symbol {
        var begin: c.LuauPosition = undefined;
        var end: c.LuauPosition = undefined;
        _ = c.luau_analysis_symbols_begin(self.handle, @intCast(i), &begin);
        _ = c.luau_analysis_symbols_end(self.handle, @intCast(i), &end);
        return .{
            .name = c.luau_analysis_symbols_name(self.handle, @intCast(i)),
            .span = .{
                .begin = .{ .line = begin.line, .column = begin.column },
                .end = .{ .line = end.line, .column = end.column },
            },
        };
    }
};

/// Collect all top-level binding declarations (locals, functions, type aliases)
/// in `src`. Returns `null` on error. Caller owns the result and must
/// `deinit` it.
pub fn symbols(src: []const u8) ?Symbols {
    const handle = c.luau_analysis_symbols(src.ptr, src.len);
    if (handle == null) return null;
    return .{ .handle = handle };
}
