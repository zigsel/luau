//! Idiomatic wrapper over `.luaurc` config parsing (via the C++ shim).

const std = @import("std");
const c = @import("bindings");

/// Type-checking mode declared by a `.luaurc`.
pub const Mode = enum(c_int) {
    nocheck = c.LUAU_MODE_NOCHECK,
    nonstrict = c.LUAU_MODE_NONSTRICT,
    strict = c.LUAU_MODE_STRICT,
    definition = c.LUAU_MODE_DEFINITION,
    _,
};

/// A parsed `.luaurc`. Owns its storage; call `deinit`.
pub const Config = struct {
    handle: *c.LuauConfig,

    pub fn deinit(self: Config) void {
        c.luau_config_free(self.handle);
    }

    /// Parse error message, or null if the config parsed cleanly.
    pub fn err(self: Config) ?[]const u8 {
        return if (c.luau_config_error(self.handle)) |p| std.mem.span(p) else null;
    }
    pub fn ok(self: Config) bool {
        return c.luau_config_error(self.handle) == null;
    }
    pub fn mode(self: Config) Mode {
        return @enumFromInt(c.luau_config_mode(self.handle));
    }
    pub fn lintErrors(self: Config) bool {
        return c.luau_config_lint_errors(self.handle) != 0;
    }
    pub fn typeErrors(self: Config) bool {
        return c.luau_config_type_errors(self.handle) != 0;
    }
    pub fn globalCount(self: Config) usize {
        return @intCast(c.luau_config_global_count(self.handle));
    }
    /// The `i`-th declared global (borrows the config's storage).
    pub fn global(self: Config, i: usize) []const u8 {
        return std.mem.span(c.luau_config_global(self.handle, @intCast(i)));
    }

    pub fn aliasCount(self: Config) usize {
        return @intCast(c.luau_config_alias_count(self.handle));
    }
    /// The `i`-th alias (all strings borrow config storage).
    pub fn alias(self: Config, i: usize) Alias {
        const idx: c_int = @intCast(i);
        return .{
            .name = std.mem.span(c.luau_config_alias_name(self.handle, idx)),
            .key = std.mem.span(c.luau_config_alias_key(self.handle, idx)),
            .value = std.mem.span(c.luau_config_alias_value(self.handle, idx)),
            .config_location = std.mem.span(c.luau_config_alias_location(self.handle, idx)),
        };
    }
    /// Iterate over declared aliases.
    pub fn aliases(self: Config) AliasIterator {
        return .{ .config = self, .i = 0, .n = self.aliasCount() };
    }
    /// Case-insensitively resolve an alias *name* (without the require-string
    /// `@` prefix) to its target value, or null if undeclared. Splitting a
    /// require path like `@pkg/mod` into `pkg` + `mod` is the caller's job.
    pub fn resolveAlias(self: Config, name: [:0]const u8) ?[]const u8 {
        return if (c.luau_config_alias_resolve(self.handle, name.ptr)) |p| std.mem.span(p) else null;
    }

    /// Enabled/fatal state of each lint rule in this config.
    pub fn lintRule(self: Config, i: usize) LintRule {
        return .{
            .name = std.mem.span(c.luau_config_lint_rule_name(@intCast(i))),
            .enabled = c.luau_config_lint_rule_enabled(self.handle, @intCast(i)) != 0,
            .fatal = c.luau_config_lint_rule_fatal(self.handle, @intCast(i)) != 0,
        };
    }
    /// Iterate over every lint rule with its enabled/fatal flags.
    pub fn lintRules(self: Config) LintRuleIterator {
        return .{ .config = self, .i = 0, .n = lintRuleCount() };
    }
};

/// A declared import alias.
pub const Alias = struct {
    /// The alias name in its original case.
    name: []const u8,
    /// The case-folded lookup key (what `require` resolves against).
    key: []const u8,
    /// The alias target value.
    value: []const u8,
    /// The `.luaurc` location this alias came from (may be empty).
    config_location: []const u8,
};

pub const AliasIterator = struct {
    config: Config,
    i: usize,
    n: usize,

    pub fn next(self: *AliasIterator) ?Alias {
        if (self.i >= self.n) return null;
        defer self.i += 1;
        return self.config.alias(self.i);
    }
};

/// A lint rule and its state within a particular config.
pub const LintRule = struct {
    name: []const u8,
    enabled: bool,
    fatal: bool,
};

pub const LintRuleIterator = struct {
    config: Config,
    i: usize,
    n: usize,

    pub fn next(self: *LintRuleIterator) ?LintRule {
        if (self.i >= self.n) return null;
        defer self.i += 1;
        return self.config.lintRule(self.i);
    }
};

/// Whether `name` is a syntactically valid alias name.
pub fn isValidAlias(name: [:0]const u8) bool {
    return c.luau_config_is_valid_alias(name.ptr) != 0;
}

/// The total number of lint rules (LintWarning::Code__Count).
pub fn lintRuleCount() usize {
    return @intCast(c.luau_config_lint_rule_count());
}

/// Parse `.luaurc` JSON `contents`.
pub fn parse(contents: []const u8) Config {
    return .{ .handle = c.luau_config_parse(contents.ptr, contents.len).? };
}
