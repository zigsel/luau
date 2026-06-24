//! Idiomatic wrapper over Luau type checking (via the C++ shim).

const std = @import("std");
const c = @import("bindings");
const Position = @import("ast.zig").Position;

// `check` (below) is the entry point: type-check + lint a single module. The
// rest of the analysis surface is a flat set of focused submodules.

// -- the inferred type graph --
/// Structural inspection of inferred `Type` / `TypePack` handles.
pub const types = @import("analysis/types.zig");
/// Type predicates, simplify/clone, and type-path traversal.
pub const typeutil = @import("analysis/typeutil.zig");
/// Canonical normalization (DNF) + structural type equality.
pub const normalize = @import("analysis/normalize.zig");
/// Subtype queries between inferred top-level binding types.
pub const subtype = @import("analysis/subtype.zig");
/// Deep transforms: instantiate, anyify, apply type functions, generalize.
pub const transform = @import("analysis/transform.zig");

// -- completion & navigation (the basic name-only `autocomplete` is below) --
/// Rich completion items: inferred type string, docs symbol, insert text,
/// deprecation flag (LSP completion).
pub const complete = @import("analysis/complete.zig");
/// Fragment (incremental) completion: reuse a stale module, re-analyse only the
/// edited region around the cursor.
pub const fragment = @import("analysis/fragment.zig");
/// In-memory documentation database, keyed by `documentationSymbol`.
pub const docs = @import("analysis/docs.zig");
/// Type at a source position (hover / LSP core).
pub const hover = @import("analysis/hover.zig");
/// Go-to-definition and top-level symbol locations.
pub const define = @import("analysis/define.zig");
/// Module/scope inspection: metadata, return type, scope bindings, exports.
pub const module = @import("analysis/module.zig");

// -- serialization (of inferred types; AST→JSON lives in `luau.ast.json`) --
/// Render inferred types: Graphviz `dot`, or `toString` with full options.
pub const viz = @import("analysis/viz.zig");

// -- the rest --
/// Multi-module type-checking with `require()` resolution.
pub const project = @import("analysis/project.zig");
/// Type-check against host type definitions (declare your API's globals).
pub const defs = @import("analysis/defs.zig");
/// The linter: rule enumeration + standalone lint with full rule control.
pub const lint = @import("analysis/lint.zig");
/// The data-flow graph builder over a parsed module.
pub const dfg = @import("analysis/dfg.zig");
/// AST-level scans needing no solver state: require-tracing and type-guard
/// matching over a parsed module.
pub const scan = @import("analysis/scan.zig");
/// Pure helpers over the type-variance `Polarity` enum.
pub const polarity = @import("analysis/polarity.zig");
/// Structured type-check diagnostics: stable kind enum, location, message, and
/// cheap typed string fields.
pub const diagnostics = @import("analysis/diagnostics.zig");
/// Host-free require-path suggestion data carriers (suggestion/alias + tags).
pub const requirepath = @import("analysis/requirepath.zig");
/// Richer documentation variants (function/table/overloaded) behind signature
/// help and structured hover.
pub const signatures = @import("analysis/signatures.zig");

/// A type error reported by the checker.
pub const TypeError = struct {
    message: []const u8,
    position: Position,
};

/// A lint warning.
pub const LintWarning = struct {
    message: []const u8,
    /// The lint rule name (e.g. "LocalShadow").
    name: []const u8,
    /// The numeric lint code.
    code: i32,
    position: Position,
};

/// The result of type-checking a module. Owns its storage; call `deinit`.
pub const CheckResult = struct {
    handle: *c.LuauCheck,

    pub fn deinit(self: CheckResult) void {
        c.luau_analysis_check_free(self.handle);
    }

    /// Whether the module type-checked with no errors.
    pub fn ok(self: CheckResult) bool {
        return self.errorCount() == 0;
    }
    pub fn errorCount(self: CheckResult) usize {
        return @intCast(c.luau_analysis_error_count(self.handle));
    }
    /// The `i`-th type error (message borrows the result's storage).
    pub fn getError(self: CheckResult, i: usize) TypeError {
        const msg = c.luau_analysis_error_message(self.handle, @intCast(i));
        const pos = c.luau_analysis_error_position(self.handle, @intCast(i));
        return .{
            .message = std.mem.span(msg),
            .position = .{ .line = pos.line, .column = pos.column },
        };
    }
    pub fn errors(self: CheckResult) ErrorIterator {
        return .{ .result = self, .n = self.errorCount() };
    }

    /// Number of lint warnings (lint is always run).
    pub fn lintCount(self: CheckResult) usize {
        return @intCast(c.luau_analysis_lint_count(self.handle));
    }
    /// The `i`-th lint warning (strings borrow the result's storage).
    pub fn getLint(self: CheckResult, i: usize) LintWarning {
        return .{
            .message = std.mem.span(c.luau_analysis_lint_message(self.handle, @intCast(i))),
            .name = std.mem.span(c.luau_analysis_lint_name(self.handle, @intCast(i))),
            .code = c.luau_analysis_lint_code(self.handle, @intCast(i)),
            .position = blk: {
                const p = c.luau_analysis_lint_position(self.handle, @intCast(i));
                break :blk .{ .line = p.line, .column = p.column };
            },
        };
    }
    pub fn lints(self: CheckResult) LintIterator {
        return .{ .result = self, .n = self.lintCount() };
    }
    pub const LintIterator = struct {
        result: CheckResult,
        i: usize = 0,
        n: usize,
        pub fn next(it: *LintIterator) ?LintWarning {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.result.getLint(it.i);
        }
    };

    pub const ErrorIterator = struct {
        result: CheckResult,
        i: usize = 0,
        n: usize,
        pub fn next(it: *ErrorIterator) ?TypeError {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.result.getError(it.i);
        }
    };
};

/// Type-check a single self-contained Luau `source` module.
pub fn check(source: []const u8) CheckResult {
    return .{ .handle = c.luau_analysis_check(source.ptr, source.len).? };
}

/// The kind of an autocomplete entry (`AutocompleteEntryKind`).
pub const EntryKind = enum(c_int) {
    property = 0,
    binding = 1,
    keyword = 2,
    string = 3,
    type = 4,
    module = 5,
    generated_function = 6,
    require_path = 7,
    hot_comment = 8,
    _,
};

pub const Entry = struct {
    name: []const u8,
    kind: EntryKind,
};

/// Autocomplete suggestions at a source position. Owns storage; call `deinit`.
pub const Autocomplete = struct {
    handle: *c.LuauAutocomplete,

    pub fn deinit(self: Autocomplete) void {
        c.luau_autocomplete_free(self.handle);
    }
    pub fn count(self: Autocomplete) usize {
        return @intCast(c.luau_autocomplete_count(self.handle));
    }
    pub fn get(self: Autocomplete, i: usize) Entry {
        return .{
            .name = std.mem.span(c.luau_autocomplete_name(self.handle, @intCast(i))),
            .kind = @enumFromInt(c.luau_autocomplete_kind(self.handle, @intCast(i))),
        };
    }
    pub fn iterator(self: Autocomplete) Iterator {
        return .{ .ac = self, .n = self.count() };
    }
    /// Whether an entry with the given name is suggested.
    pub fn contains(self: Autocomplete, name: []const u8) bool {
        var it = self.iterator();
        while (it.next()) |e| if (std.mem.eql(u8, e.name, name)) return true;
        return false;
    }
    pub const Iterator = struct {
        ac: Autocomplete,
        i: usize = 0,
        n: usize,
        pub fn next(it: *Iterator) ?Entry {
            if (it.i >= it.n) return null;
            defer it.i += 1;
            return it.ac.get(it.i);
        }
    };
};

/// Compute autocomplete suggestions at (0-based) `line`/`column` in `source`.
pub fn autocomplete(source: []const u8, line: u32, column: u32) Autocomplete {
    return .{ .handle = c.luau_autocomplete(source.ptr, source.len, line, column).? };
}
