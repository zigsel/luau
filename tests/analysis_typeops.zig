//! Analysis — operations over the inferred type graph: type utilities & paths,
//! visualization, module/scope inspection, normalization, transforms, plus the
//! self-contained scan (require-tracing / type-guards) and polarity helpers.
//!
//! Tolerant: these are solver-dependent, so we assert "the accessors run" and
//! exercise every shimmed entry point rather than hard-asserting inferred shape.

const std = @import("std");
const luau = @import("luau");
const analysis = luau.analysis;
const types = analysis.types;
const typeutils = analysis.typeutil;
const viz = analysis.viz;
const module = analysis.module;
const normalize = analysis.normalize;
const transforms = analysis.transform;
const scan = analysis.scan;
const polarity = analysis.polarity;

const testing = std.testing;
const alloc = testing.allocator;

fn someType(checker: types.Checker) ?types.Type {
    return checker.global("n");
}

test "typeutils: predicates and utilities run on an inferred type" {
    var checker = types.check("local n = 1\nreturn n");
    defer checker.deinit();

    const t = someType(checker) orelse return;
    const f = typeutils.follow(t) orelse t;

    _ = typeutils.isNil(f);
    _ = typeutils.isBoolean(f);
    _ = typeutils.isNumber(f);
    _ = typeutils.isInteger(f);
    _ = typeutils.isString(f);
    _ = typeutils.isThread(f);
    _ = typeutils.isBuffer(f);
    _ = typeutils.isOptional(f);
    _ = typeutils.isTableUnion(f);
    _ = typeutils.isTableIntersection(f);
    _ = typeutils.isOverloadedFunction(f);
    _ = typeutils.maybeSingleton(f);
    _ = typeutils.isGeneric(f);
    _ = typeutils.isPrim(f, .number);
    _ = typeutils.fastIsSubtype(f, f);
    _ = typeutils.isApproximatelyFalsy(f);
    _ = typeutils.isApproximatelyTruthy(f);
    _ = typeutils.isBlocked(f);

    _ = typeutils.isOptionalType(checker, f);
    _ = typeutils.stripNil(checker, f);
    _ = typeutils.approximateReturn(f);
    _ = typeutils.simplifyUnion(checker, f, f);
    _ = typeutils.simplifyIntersection(checker, f, f);
    _ = typeutils.relate(f, f);
    _ = typeutils.clone(checker, f, false);
    _ = typeutils.clone(checker, f, true);
}

test "typeutils: type path build / stringify / traverse" {
    var checker = types.check("local n = 1\nreturn n");
    defer checker.deinit();

    const t = someType(checker) orelse return;

    var path = typeutils.Path.init() orelse return;
    defer path.deinit();
    _ = path.readProp("x").index(0);

    const human = try path.toString(alloc, true);
    defer alloc.free(human);
    const dbg = try path.toString(alloc, false);
    defer alloc.free(dbg);

    _ = path.traverseToType(t);
    _ = path.traverseToPack(t);
}

test "viz: toDot and toString-with-options produce non-empty strings" {
    var checker = types.check("local n = 1\nreturn n");
    defer checker.deinit();

    const t = someType(checker) orelse return;

    const dot = try viz.typeToDot(alloc, t, .{});
    defer alloc.free(dot);
    try testing.expect(dot.len > 0);

    const s = try viz.typeToString(alloc, t, .{});
    defer alloc.free(s);
    try testing.expect(s.len > 0);

    const s2 = try viz.typeToString(alloc, t, .{ .exhaustive = true, .use_line_breaks = true });
    defer alloc.free(s2);
    try testing.expect(s2.len > 0);

    const detailed = try viz.typeToStringDetailed(alloc, t, .{});
    defer alloc.free(detailed.string);
    try testing.expect(detailed.string.len > 0);
}

test "viz: type pack rendering (tolerant)" {
    var checker = types.check("local function f(): number return 1 end\nreturn f");
    defer checker.deinit();

    const t = checker.global("f") orelse return;
    const f = typeutils.follow(t) orelse t;
    const rets = f.functionRets() orelse return;

    const dot = try viz.typePackToDot(alloc, rets, .{});
    defer alloc.free(dot);
    const s = try viz.typePackToString(alloc, rets, .{});
    defer alloc.free(s);
}

test "module: check, bindings, return type, exported types" {
    var mod = module.Module.check("local x = 1\nreturn x");
    defer mod.deinit();

    _ = mod.checked();
    _ = mod.errorCount();
    _ = mod.timedOut();
    _ = mod.cancelled();

    if (try mod.name(alloc)) |nm| alloc.free(nm);
    if (try mod.humanName(alloc)) |nm| alloc.free(nm);

    var errs = mod.errors();
    while (errs.next()) |_| {}

    var it = mod.bindings();
    while (it.next()) |b| {
        try testing.expect(b.name.len >= 0);
        _ = b.type;
        _ = b.location;
        _ = b.deprecated;
    }
    _ = mod.bindingCount();
    _ = mod.lookup("x");
    _ = mod.returnType();

    var ex = mod.exportedTypes();
    while (ex.next()) |_| {}
    _ = mod.exportedTypeCount();
}

test "normalize: normalize / tostring / structural equality" {
    var checker = types.check("local n = 1\nreturn n");
    defer checker.deinit();

    const t = someType(checker) orelse return;
    _ = normalize.normalize(t);
    if (try normalize.toStringAlloc(alloc, t)) |s| alloc.free(s);
    _ = normalize.structurallyEqual(t, t);
}

test "transforms: instantiate / anyify / applyTypeFunction / generalize run" {
    var checker = types.check(
        \\local function id(x) return x end
        \\local n = 1
        \\return id
    );
    defer checker.deinit();

    const t = someType(checker) orelse return;

    _ = transforms.instantiate(checker, t);
    _ = transforms.anyify(checker, t);
    const args = [_]types.Type{t};
    _ = transforms.applyTypeFunction(checker, t, &args);
    _ = transforms.generalize(checker, t);
}

test "scan: parse / require tracing / type-guard scanning" {
    var parsed = scan.Scan.parse(
        \\local m = require("./mod")
        \\local function f(x)
        \\    if typeof(x) == "string" then return x end
        \\    return m
        \\end
        \\return f
    );
    defer parsed.deinit();

    _ = parsed.hasRoot();
    const ec = parsed.errorCount();
    if (ec > 0) _ = parsed.errorMessage(0);

    const requires = parsed.traceRequires(alloc) catch null;
    if (requires) |rs| {
        defer alloc.free(rs);
        for (rs) |r| alloc.free(r.name);
    }
    _ = parsed.requireCount();

    const guards = parsed.scanTypeGuards(alloc) catch null;
    if (guards) |gs| {
        defer alloc.free(gs);
        for (gs) |g| alloc.free(g.type);
    }
    _ = parsed.typeGuardCount();
}

test "polarity: bit helpers" {
    const p = polarity.Polarity.positive;
    _ = p.isPositive();
    _ = p.isNegative();
    _ = p.isKnown();
    _ = p.invert();
    _ = p.unionWith(polarity.Polarity.negative);
    _ = p.intersect(polarity.Polarity.mixed);
}
