//! Tests for the host-free require-path suggestion data carriers
//! (`Suggestion` / `Alias`). The abstract resolver machinery is not bound, so
//! these only exercise the POD round-trip.

const std = @import("std");
const luau = @import("luau");

const requirepath = luau.analysis.requirepath;

test "require suggestion: round-trips label, full path and tags" {
    const s = requirepath.Suggestion.init("Module", "@self/Module");
    defer s.deinit();

    try std.testing.expectEqualStrings("Module", s.label());
    try std.testing.expectEqualStrings("@self/Module", s.fullPath());
    try std.testing.expectEqual(@as(usize, 0), s.tagCount());

    s.addTag("file");
    s.addTag("relative");
    try std.testing.expectEqual(@as(usize, 2), s.tagCount());
    try std.testing.expectEqualStrings("file", s.tag(0));
    try std.testing.expectEqualStrings("relative", s.tag(1));
    try std.testing.expectEqualStrings("", s.tag(2));

    var it = s.tags();
    try std.testing.expectEqualStrings("file", it.next().?);
    try std.testing.expectEqualStrings("relative", it.next().?);
    try std.testing.expect(it.next() == null);
}

test "require alias: round-trips name and tags" {
    const a = requirepath.Alias.init("lib");
    defer a.deinit();

    try std.testing.expectEqualStrings("lib", a.name());
    try std.testing.expectEqual(@as(usize, 0), a.tagCount());

    a.addTag("workspace");
    try std.testing.expectEqual(@as(usize, 1), a.tagCount());
    try std.testing.expectEqualStrings("workspace", a.tag(0));

    var it = a.tags();
    try std.testing.expectEqualStrings("workspace", it.next().?);
    try std.testing.expect(it.next() == null);
}
