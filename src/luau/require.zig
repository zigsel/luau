//! Require-by-string (`Luau/Require.h`), wrapped idiomatically.
//!
//! Luau's require is driven by a *navigation* protocol: you describe your module
//! hierarchy (filesystem, virtual FS, …) and Luau walks it. You implement a
//! `Resolver` — a context pointer + a `VTable` of methods — and `install` it; the
//! raw C `Configuration` of callbacks and its `void*` context never surface.
//!
//! The `VTable` lists exactly what you must implement: the 10 non-optional fields
//! are required, the trailing `?`-typed fields are optional (leave them `null`).

const std = @import("std");
const c = @import("bindings");
const Lua = @import("../lua/lua.zig").Lua;

pub const NavigateResult = enum(c_int) {
    success = c.NAVIGATE_SUCCESS,
    ambiguous = c.NAVIGATE_AMBIGUOUS,
    not_found = c.NAVIGATE_NOT_FOUND,
    _,
};
pub const ConfigStatus = enum(c_int) {
    absent = c.CONFIG_ABSENT,
    ambiguous = c.CONFIG_AMBIGUOUS,
    present_json = c.CONFIG_PRESENT_JSON,
    present_luau = c.CONFIG_PRESENT_LUAU,
    _,
};

/// A require resolver: your state (`ptr`) plus the `VTable` describing how to
/// navigate your module hierarchy.
pub const Resolver = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    /// The navigation protocol. `ctx` is your `Resolver.ptr`; cast it back with
    /// `@ptrCast(@alignCast(ctx))`. Strings passed in borrow Luau storage (valid
    /// for the call); strings you return must stay valid until the next call.
    pub const VTable = struct {
        // -- required --
        /// May a require run from this chunk?
        isRequireAllowed: *const fn (ctx: *anyopaque, lua: *Lua, requirer_chunkname: []const u8) bool,
        /// Point internal state at the requiring module.
        reset: *const fn (ctx: *anyopaque, lua: *Lua, requirer_chunkname: []const u8) NavigateResult,
        /// Move to the parent context.
        toParent: *const fn (ctx: *anyopaque, lua: *Lua) NavigateResult,
        /// Move to the named child.
        toChild: *const fn (ctx: *anyopaque, lua: *Lua, name: []const u8) NavigateResult,
        /// Is the current context a real module?
        isModulePresent: *const fn (ctx: *anyopaque, lua: *Lua) bool,
        /// Debug chunkname for the current module.
        getChunkname: *const fn (ctx: *anyopaque, lua: *Lua) []const u8,
        /// Loadname passed to `load`.
        getLoadname: *const fn (ctx: *anyopaque, lua: *Lua) []const u8,
        /// Cache key uniquely identifying the current module.
        getCacheKey: *const fn (ctx: *anyopaque, lua: *Lua) []const u8,
        /// Whether a `.luaurc` is present here (and its syntax).
        getConfigStatus: *const fn (ctx: *anyopaque, lua: *Lua) ConfigStatus,
        /// Execute the current module: push its result(s), return the count
        /// (or -1 to yield).
        load: *const fn (ctx: *anyopaque, lua: *Lua, path: []const u8, chunkname: []const u8, loadname: []const u8) i32,

        // -- optional (leave null if unused) --
        /// Resolve an absolute alias path (when it can't be resolved relatively).
        jumpToAlias: ?*const fn (ctx: *anyopaque, lua: *Lua, path: []const u8) NavigateResult = null,
        /// First chance to resolve an alias, before config-file search.
        toAliasOverride: ?*const fn (ctx: *anyopaque, lua: *Lua, alias: []const u8) NavigateResult = null,
        /// Last chance to resolve an alias, after config-file search.
        toAliasFallback: ?*const fn (ctx: *anyopaque, lua: *Lua, alias: []const u8) NavigateResult = null,
        /// Contents of the `.luaurc` here (Luau parses it). Only called when
        /// `getConfigStatus` is not `.absent`.
        getConfig: ?*const fn (ctx: *anyopaque, lua: *Lua) []const u8 = null,
        /// Timeout (ms) for executing a Luau-syntax `.luaurc` (default 2000).
        getLuauConfigTimeout: ?*const fn (ctx: *anyopaque, lua: *Lua) i32 = null,
    };
};

// ---- bridge: fixed C trampolines that dispatch through the vtable -----------

inline fn resolverOf(ctx: ?*anyopaque) *const Resolver {
    return @ptrCast(@alignCast(ctx.?));
}
inline fn span(s: [*c]const u8) []const u8 {
    return if (s) |p| std.mem.span(p) else "";
}
fn nav(r: NavigateResult) c.luarequire_NavigateResult {
    return switch (r) {
        .ambiguous => c.NAVIGATE_AMBIGUOUS,
        .not_found => c.NAVIGATE_NOT_FOUND,
        else => c.NAVIGATE_SUCCESS,
    };
}
/// Copy `s` into the caller's buffer per the C write protocol.
fn writeOut(s: []const u8, buffer: [*c]u8, size: usize, out: [*c]usize) c.luarequire_WriteResult {
    out.* = s.len;
    if (s.len > size) return c.WRITE_BUFFER_TOO_SMALL;
    if (s.len > 0) @memcpy(buffer[0..s.len], s);
    return c.WRITE_SUCCESS;
}

const Tramp = struct {
    fn isRequireAllowed(L: ?*c.lua_State, ctx: ?*anyopaque, chunk: [*c]const u8) callconv(.c) bool {
        const r = resolverOf(ctx);
        return r.vtable.isRequireAllowed(r.ptr, Lua.fromRaw(L.?), span(chunk));
    }
    fn reset(L: ?*c.lua_State, ctx: ?*anyopaque, chunk: [*c]const u8) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(r.vtable.reset(r.ptr, Lua.fromRaw(L.?), span(chunk)));
    }
    fn jumpToAlias(L: ?*c.lua_State, ctx: ?*anyopaque, path: [*c]const u8) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(if (r.vtable.jumpToAlias) |f| f(r.ptr, Lua.fromRaw(L.?), span(path)) else .not_found);
    }
    fn toAliasOverride(L: ?*c.lua_State, ctx: ?*anyopaque, alias: [*c]const u8) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(if (r.vtable.toAliasOverride) |f| f(r.ptr, Lua.fromRaw(L.?), span(alias)) else .not_found);
    }
    fn toAliasFallback(L: ?*c.lua_State, ctx: ?*anyopaque, alias: [*c]const u8) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(if (r.vtable.toAliasFallback) |f| f(r.ptr, Lua.fromRaw(L.?), span(alias)) else .not_found);
    }
    fn toParent(L: ?*c.lua_State, ctx: ?*anyopaque) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(r.vtable.toParent(r.ptr, Lua.fromRaw(L.?)));
    }
    fn toChild(L: ?*c.lua_State, ctx: ?*anyopaque, name: [*c]const u8) callconv(.c) c.luarequire_NavigateResult {
        const r = resolverOf(ctx);
        return nav(r.vtable.toChild(r.ptr, Lua.fromRaw(L.?), span(name)));
    }
    fn isModulePresent(L: ?*c.lua_State, ctx: ?*anyopaque) callconv(.c) bool {
        const r = resolverOf(ctx);
        return r.vtable.isModulePresent(r.ptr, Lua.fromRaw(L.?));
    }
    fn getChunkname(L: ?*c.lua_State, ctx: ?*anyopaque, buf: [*c]u8, size: usize, out: [*c]usize) callconv(.c) c.luarequire_WriteResult {
        const r = resolverOf(ctx);
        return writeOut(r.vtable.getChunkname(r.ptr, Lua.fromRaw(L.?)), buf, size, out);
    }
    fn getLoadname(L: ?*c.lua_State, ctx: ?*anyopaque, buf: [*c]u8, size: usize, out: [*c]usize) callconv(.c) c.luarequire_WriteResult {
        const r = resolverOf(ctx);
        return writeOut(r.vtable.getLoadname(r.ptr, Lua.fromRaw(L.?)), buf, size, out);
    }
    fn getCacheKey(L: ?*c.lua_State, ctx: ?*anyopaque, buf: [*c]u8, size: usize, out: [*c]usize) callconv(.c) c.luarequire_WriteResult {
        const r = resolverOf(ctx);
        return writeOut(r.vtable.getCacheKey(r.ptr, Lua.fromRaw(L.?)), buf, size, out);
    }
    fn getConfigStatus(L: ?*c.lua_State, ctx: ?*anyopaque) callconv(.c) c.luarequire_ConfigStatus {
        const r = resolverOf(ctx);
        return switch (r.vtable.getConfigStatus(r.ptr, Lua.fromRaw(L.?))) {
            .present_json => c.CONFIG_PRESENT_JSON,
            .present_luau => c.CONFIG_PRESENT_LUAU,
            .ambiguous => c.CONFIG_AMBIGUOUS,
            else => c.CONFIG_ABSENT,
        };
    }
    fn getConfig(L: ?*c.lua_State, ctx: ?*anyopaque, buf: [*c]u8, size: usize, out: [*c]usize) callconv(.c) c.luarequire_WriteResult {
        const r = resolverOf(ctx);
        if (r.vtable.getConfig) |f| return writeOut(f(r.ptr, Lua.fromRaw(L.?)), buf, size, out);
        out.* = 0;
        return c.WRITE_FAILURE;
    }
    fn getLuauConfigTimeout(L: ?*c.lua_State, ctx: ?*anyopaque) callconv(.c) c_int {
        const r = resolverOf(ctx);
        return if (r.vtable.getLuauConfigTimeout) |f| f(r.ptr, Lua.fromRaw(L.?)) else 2000;
    }
    fn load(L: ?*c.lua_State, ctx: ?*anyopaque, path: [*c]const u8, chunk: [*c]const u8, loadn: [*c]const u8) callconv(.c) c_int {
        const r = resolverOf(ctx);
        return r.vtable.load(r.ptr, Lua.fromRaw(L.?), span(path), span(chunk), span(loadn));
    }
};

fn configInit(config: [*c]c.luarequire_Configuration) callconv(.c) void {
    // All trampolines are always set; optional ones dispatch-or-default through
    // the vtable at call time. (get_alias is intentionally not exposed — its
    // manual-config-parsing mode is mutually exclusive with get_config.)
    config.* = .{
        .is_require_allowed = Tramp.isRequireAllowed,
        .reset = Tramp.reset,
        .jump_to_alias = Tramp.jumpToAlias,
        .to_alias_override = Tramp.toAliasOverride,
        .to_alias_fallback = Tramp.toAliasFallback,
        .to_parent = Tramp.toParent,
        .to_child = Tramp.toChild,
        .is_module_present = Tramp.isModulePresent,
        .get_chunkname = Tramp.getChunkname,
        .get_loadname = Tramp.getLoadname,
        .get_cache_key = Tramp.getCacheKey,
        .get_config_status = Tramp.getConfigStatus,
        .get_alias = null,
        .get_config = Tramp.getConfig,
        .get_luau_config_timeout = Tramp.getLuauConfigTimeout,
        .load = Tramp.load,
    };
}

inline fn ctxOf(resolver: *const Resolver) ?*anyopaque {
    return @constCast(@ptrCast(resolver));
}

// ---- public API -------------------------------------------------------------

/// Install a configurable `require` global driven by `resolver`. The resolver
/// (and the state it points at) must outlive the VM.
pub fn install(lua: *Lua, resolver: *const Resolver) void {
    c.luaopen_require(lua.toRaw(), configInit, ctxOf(resolver));
}
/// Push a `require` closure onto the stack (without making it global).
pub fn pushRequire(lua: *Lua, resolver: *const Resolver) i32 {
    return c.luarequire_pushrequire(lua.toRaw(), configInit, ctxOf(resolver));
}
/// Push a "proxy require" closure (resolves a path as if required from a given
/// module's chunkname); takes `(path, chunkname)` as stack arguments.
pub fn pushProxyRequire(lua: *Lua, resolver: *const Resolver) i32 {
    return c.luarequire_pushproxyrequire(lua.toRaw(), configInit, ctxOf(resolver));
}

/// Register the module on top of the stack into the require cache (expects the
/// path and table on the stack).
pub fn registerModule(lua: *Lua) i32 {
    return c.luarequire_registermodule(lua.toRaw());
}
/// Clear one cache entry (expects the cache key on the stack).
pub fn clearCacheEntry(lua: *Lua) i32 {
    return c.luarequire_clearcacheentry(lua.toRaw());
}
/// Clear the entire require cache.
pub fn clearCache(lua: *Lua) i32 {
    return c.luarequire_clearcache(lua.toRaw());
}

// ---- a ready-made, sandboxed filesystem resolver ---------------------------

/// A configurable `Resolver` that loads modules from a directory and **cannot
/// escape it**: navigation past the root fails, so `../` can never reach outside
/// `root`. All filesystem access goes through the `std.Io` you pass.
///
///     var threaded = std.Io.Threaded.init(gpa, .{});
///     var fs = luau.require.FsResolver.init(threaded.io(), gpa, .{ .root = "mods" });
///     var r = fs.resolver();
///     luau.require.install(vm, &r);
pub const FsResolver = struct {
    pub const Options = struct {
        /// Directory modules resolve within (the jail root).
        root: []const u8,
        /// File extensions tried, in order.
        extensions: []const []const u8 = &.{ "luau", "lua" },
        /// Largest module file read.
        max_bytes: usize = 1 << 20,
        /// Optional per-module gate: return false to hide a module (by its
        /// root-relative path, e.g. "sub/mod"). Lets you deny subtrees.
        allow: ?*const fn (path: []const u8) bool = null,
    };

    io: std.Io,
    gpa: std.mem.Allocator,
    opts: Options,
    buf: [1024]u8 = undefined,
    cur: []const u8 = "",

    pub fn init(io: std.Io, gpa: std.mem.Allocator, opts: Options) FsResolver {
        return .{ .io = io, .gpa = gpa, .opts = opts };
    }
    /// The `Resolver` to hand to `install`/`pushRequire`.
    pub fn resolver(self: *FsResolver) Resolver {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ctx: *anyopaque) *FsResolver {
        return @ptrCast(@alignCast(ctx));
    }
    fn set(v: *FsResolver, s: []const u8) void {
        std.mem.copyForwards(u8, v.buf[0..s.len], s);
        v.cur = v.buf[0..s.len];
    }
    /// Open the current module's file (first matching extension), or null.
    fn openCurrent(v: *FsResolver) ?std.Io.File {
        if (v.opts.allow) |gate| if (!gate(v.cur)) return null;
        var pb: [1100]u8 = undefined;
        for (v.opts.extensions) |ext| {
            const path = std.fmt.bufPrint(&pb, "{s}/{s}.{s}", .{ v.opts.root, v.cur, ext }) catch continue;
            if (std.Io.Dir.cwd().openFile(v.io, path, .{})) |f| return f else |_| {}
        }
        return null;
    }

    fn isRequireAllowed(_: *anyopaque, _: *Lua, _: []const u8) bool {
        return true;
    }
    fn reset(ctx: *anyopaque, _: *Lua, requirer_chunkname: []const u8) NavigateResult {
        var nm = requirer_chunkname;
        if (nm.len > 0 and (nm[0] == '=' or nm[0] == '@')) nm = nm[1..];
        if (nm.len > from(ctx).buf.len) return .not_found;
        from(ctx).set(nm);
        return .success;
    }
    fn toParent(ctx: *anyopaque, _: *Lua) NavigateResult {
        const v = from(ctx);
        if (v.cur.len == 0) return .not_found; // jail boundary — cannot escape root
        if (std.mem.lastIndexOfScalar(u8, v.cur, '/')) |i| v.set(v.cur[0..i]) else v.set("");
        return .success;
    }
    fn toChild(ctx: *anyopaque, _: *Lua, name: []const u8) NavigateResult {
        const v = from(ctx);
        if (std.mem.eql(u8, name, ".")) return .success;
        // reject anything that could escape or confuse the jail
        if (name.len == 0 or std.mem.eql(u8, name, "..")) return .not_found;
        if (std.mem.indexOfAny(u8, name, "/\\") != null) return .not_found;
        if (v.cur.len == 0) {
            v.set(name);
        } else {
            var tmp: [1024]u8 = undefined;
            v.set(std.fmt.bufPrint(&tmp, "{s}/{s}", .{ v.cur, name }) catch return .not_found);
        }
        return .success;
    }
    fn isModulePresent(ctx: *anyopaque, _: *Lua) bool {
        const v = from(ctx);
        if (v.openCurrent()) |f| {
            f.close(v.io);
            return true;
        }
        return false;
    }
    fn curName(ctx: *anyopaque, _: *Lua) []const u8 {
        return from(ctx).cur;
    }
    fn configStatus(_: *anyopaque, _: *Lua) ConfigStatus {
        return .absent;
    }
    fn load(ctx: *anyopaque, lua: *Lua, _: []const u8, _: []const u8, _: []const u8) i32 {
        const v = from(ctx);
        var pb: [1100]u8 = undefined;
        for (v.opts.extensions) |ext| {
            const path = std.fmt.bufPrint(&pb, "{s}/{s}.{s}", .{ v.opts.root, v.cur, ext }) catch continue;
            const src = std.Io.Dir.cwd().readFileAlloc(v.io, path, v.gpa, .limited(v.opts.max_bytes)) catch continue;
            defer v.gpa.free(src);
            lua.loadString("=module", src) catch return 0;
            lua.pcall(0, 1, 0) catch return 0;
            return 1;
        }
        return 0;
    }

    const vtable = Resolver.VTable{
        .isRequireAllowed = isRequireAllowed,
        .reset = reset,
        .toParent = toParent,
        .toChild = toChild,
        .isModulePresent = isModulePresent,
        .getChunkname = curName,
        .getLoadname = curName,
        .getCacheKey = curName,
        .getConfigStatus = configStatus,
        .load = load,
    };
};
