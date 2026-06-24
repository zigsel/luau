//! Self-contained scans over a parsed module that need no live solver state:
//!
//! * `traceRequires` — collect every statically-known `require(...)` target
//!   (the RequireTracer pass) as `(name, position)` pairs.
//! * `scanTypeGuards` — find binary expressions recognised by AstUtils'
//!   `matchTypeGuard` (e.g. `typeof(x) == "string"`) as `(isTypeof, type,
//!   position)`.
//!
//! Deeper Analysis headers (OverloadResolver, Refinement, Predicate,
//! TableLiteralInference, TypeFunction(Runtime)) need live solver/runtime state
//! and are intentionally not bound; see shim/analysis/scan.cpp for rationale.
//! (The Polarity bit helpers that used to live here are now `analysis.polarity`.)

const std = @import("std");
const c = @import("bindings");
const Position = @import("../ast.zig").Position;

fn toPos(p: c.LuauPosition) Position {
    return .{ .line = p.line, .column = p.column };
}

pub const Require = struct {
    /// Module name owned by `allocator` (free with `allocator.free`).
    name: []u8,
    position: Position,
};

pub const TypeGuard = struct {
    /// True if the guard used `typeof(...)`, false for `type(...)`.
    is_typeof: bool,
    /// Target type string owned by `allocator` (free with `allocator.free`).
    type: []u8,
    position: Position,
};

pub const Scan = struct {
    handle: *c.LuauScan,

    /// Parse `src`. Never fails to allocate a handle; check `hasRoot`.
    pub fn parse(src: []const u8) Scan {
        return .{ .handle = c.luau_scan_parse(src.ptr, src.len).? };
    }

    pub fn deinit(self: Scan) void {
        c.luau_scan_free(self.handle);
    }

    /// Whether parsing produced a usable statement-block root.
    pub fn hasRoot(self: Scan) bool {
        return c.luau_scan_has_root(self.handle) != 0;
    }

    pub fn errorCount(self: Scan) usize {
        return @intCast(c.luau_scan_error_count(self.handle));
    }

    /// Number of require targets materialized by the most recent
    /// `traceRequires` call (0 before any call).
    pub fn requireCount(self: Scan) usize {
        const n = c.luau_scan_require_count(self.handle);
        return if (n < 0) 0 else @intCast(n);
    }

    /// Number of type guards materialized by the most recent `scanTypeGuards`
    /// call (0 before any call).
    pub fn typeGuardCount(self: Scan) usize {
        const n = c.luau_scan_type_guard_count(self.handle);
        return if (n < 0) 0 else @intCast(n);
    }

    /// Parse error message `i`, borrowed (valid until `deinit`); null if OOB.
    pub fn errorMessage(self: Scan, i: usize) ?[:0]const u8 {
        const m = c.luau_scan_error_message(self.handle, @intCast(i)) orelse return null;
        return std.mem.span(m);
    }

    /// Run the RequireTracer pass and return the require targets, each owned by
    /// `allocator`. Returns `error.NoRoot` if the parse produced no root.
    pub fn traceRequires(self: Scan, allocator: std.mem.Allocator) ![]Require {
        const n = c.luau_scan_trace_requires(self.handle);
        if (n < 0) return error.NoRoot;

        const count: usize = @intCast(n);
        const out = try allocator.alloc(Require, count);
        errdefer allocator.free(out);

        var filled: usize = 0;
        errdefer for (out[0..filled]) |r| allocator.free(r.name);

        while (filled < count) : (filled += 1) {
            const cname = c.luau_scan_require_name(self.handle, @intCast(filled)) orelse return error.OutOfRange;
            defer std.c.free(cname);
            out[filled] = .{
                .name = try allocator.dupe(u8, std.mem.span(cname)),
                .position = toPos(c.luau_scan_require_position(self.handle, @intCast(filled))),
            };
        }
        return out;
    }

    /// Scan for `matchTypeGuard`-recognised binary expressions. Each result's
    /// `type` is owned by `allocator`. Returns `error.NoRoot` if no root.
    pub fn scanTypeGuards(self: Scan, allocator: std.mem.Allocator) ![]TypeGuard {
        const n = c.luau_scan_type_guards(self.handle);
        if (n < 0) return error.NoRoot;

        const count: usize = @intCast(n);
        const out = try allocator.alloc(TypeGuard, count);
        errdefer allocator.free(out);

        var filled: usize = 0;
        errdefer for (out[0..filled]) |g| allocator.free(g.type);

        while (filled < count) : (filled += 1) {
            const ctype = c.luau_scan_type_guard_type(self.handle, @intCast(filled)) orelse return error.OutOfRange;
            defer std.c.free(ctype);
            out[filled] = .{
                .is_typeof = c.luau_scan_type_guard_is_typeof(self.handle, @intCast(filled)) == 1,
                .type = try allocator.dupe(u8, std.mem.span(ctype)),
                .position = toPos(c.luau_scan_type_guard_position(self.handle, @intCast(filled))),
            };
        }
        return out;
    }
};
