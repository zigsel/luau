#!/usr/bin/env python3
# Enumerate Luau's *public, callable* C++ API by parsing the real clang AST
# (not grep). For each module it compiles the public headers, walks the AST, and
# lists public free functions + public class methods, classifying each as
# "bindable" (params/return are simple/POD/handle-able) or "internal" (mentions
# solver/arena/NotNull/etc. types that aren't sensible to bind).
#
# usage: tools/api_inventory.py [luau-src-root] [--list MODULE]
import json, subprocess, sys, os, tempfile

LUAU = "/tmp/luau-src"
LIST_MODULE = None
GAPS = False
GAPS_MODULE = None
args = sys.argv[1:]
i = 0
while i < len(args):
    if args[i] == "--list":
        LIST_MODULE = args[i + 1]; i += 2
    elif args[i] == "--gaps":
        GAPS = True
        # optional module name following --gaps to dump that module's untouched list
        if i + 1 < len(args) and args[i + 1][:1].isupper():
            GAPS_MODULE = args[i + 1]; i += 2
        else:
            i += 1
    else:
        LUAU = args[i]; i += 1

# our shim sources live alongside this script's repo root
SHIM = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "shim")

MODULES = ["Ast", "Compiler", "Config", "Bytecode", "CodeGen", "Analysis"]
INCLUDES = [f"{LUAU}/{m}/include" for m in MODULES] + [
    f"{LUAU}/Common/include", f"{LUAU}/VM/include",
]

# type fragments that mark a declaration as internal / not worth binding
INTERNAL = [
    "TxnLog", "ConstraintSolver", "ConstraintGenerator", "NotNull", "DenseHash",
    "Variant<", "TypeArena", "Scope>", "ScopePtr", "Unifier", "Normalizer",
    "RecursionCounter", "InternalErrorReporter", "TypeIds", "BuiltinTypes",
    "UnifierSharedState", "TypeCheckLimits", "TypeFunctionContext", "Constraint",
    "DataFlowGraph", "ControlFlow", "Substitution", "Anyification", "Polarity",
    "std::shared_ptr", "DcrLogger", "TypeFunctionRuntime", "SimplifierPtr",
]

def headers(incdir):
    out = []
    for root, _, files in os.walk(incdir):
        for f in files:
            if f.endswith((".h", ".hpp")):
                out.append(os.path.join(root, f))
    return sorted(out)

def ast_dump(module):
    incdir = f"{LUAU}/{module}/include"
    src = "".join(f'#include "{h}"\n' for h in headers(incdir))
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as tf:
        tf.write(src); path = tf.name
    cmd = ["clang++", "-std=c++17", "-fsyntax-only", "-ferror-limit=0",
           "-Xclang", "-ast-dump=json"]
    for inc in INCLUDES:
        cmd += ["-I", inc]
    cmd.append(path)
    res = subprocess.run(cmd, capture_output=True, text=True)
    os.unlink(path)
    try:
        return json.loads(res.stdout)
    except Exception:
        return None

def collect(node, curfile, incdir, acc):
    loc = node.get("loc") or {}
    if "file" in loc:
        curfile[0] = loc["file"]
    elif "includedFrom" in loc and "file" in (loc.get("includedFrom") or {}):
        pass  # keep current
    kind = node.get("kind", "")
    if kind in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl"):
        access = node.get("access", "public")
        f = curfile[0] or ""
        if access == "public" and incdir in f:
            name = node.get("name", "")
            qt = (node.get("type") or {}).get("qualType", "")
            if name and not name.startswith("~") and not name.startswith("operator"):
                acc.append((os.path.basename(f), name, qt, kind))
    for c in node.get("inner", []) or []:
        collect(c, curfile, incdir, acc)

def collect_all(tree, m):
    acc = []
    collect(tree, [None], f"{LUAU}/{m}/include", acc)
    return acc

# --- gap analysis: which public classes / free functions are referenced at all
#     in our shim sources for the module (class names are distinctive, so this is
#     a low-false-positive signal for "whole area not bound") ---

import re

# STL / libc++ template leakage and internal functor helpers that clang attributes
# to a Luau header (via using-aliases / DenseHash internals) but are not Luau API.
STL_NOISE = {
    "unordered_set", "unordered_multiset", "unordered_map", "vector", "pair",
    "map", "set", "string", "optional", "variant", "array", "tuple",
}
def is_noise(name):
    if name.startswith("_"):
        return True
    if name in STL_NOISE:
        return True
    # pure hash/equality functor helpers (ConstantKeyHash, StringRefHash, ...)
    if name.endswith(("Hash", "HashDefault", "Eq", "Equal", "Less")):
        return True
    return False

def collect_decls(node, curfile, incdir, records, freefns):
    loc = node.get("loc") or {}
    if "file" in loc:
        curfile[0] = loc["file"]
    kind = node.get("kind", "")
    f = curfile[0] or ""
    inside = incdir in f
    if kind == "CXXRecordDecl" and inside:
        name = node.get("name", "")
        # a definition (has inner), named, an actual class/struct, not STL/libc++
        # leakage (_-prefixed, std containers) or pure hash/eq functor helpers
        if name and name[0].isupper() and node.get("inner") \
                and node.get("tagUsed") in ("class", "struct") \
                and not node.get("isImplicit") and not is_noise(name):
            records.add(name)  # PascalCase only — Luau classes; drops std snake_case traits
    if kind == "FunctionDecl" and inside and node.get("access", "public") == "public":
        name = node.get("name", "")
        if name and not name.startswith("operator") and not is_noise(name):
            freefns.add(name)
    for c in node.get("inner", []) or []:
        collect_decls(c, curfile, incdir, records, freefns)

def shim_tokens(module):
    sub = os.path.join(SHIM, module.lower())
    text = []
    if os.path.isdir(sub):
        for root, _, files in os.walk(sub):
            for fn in files:
                if fn.endswith((".cpp", ".h", ".hpp")):
                    try:
                        with open(os.path.join(root, fn), encoding="utf-8", errors="ignore") as fh:
                            text.append(fh.read())
                    except OSError:
                        pass
    return set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", "\n".join(text)))

def is_bindable(qt):
    return not any(h in qt for h in INTERNAL)

print(f"Luau public C++ API inventory (clang AST)  src={LUAU}\n")
print(f"  Splits the public surface into what MATTERS (free functions + meaningful")
print(f"  methods) vs noise (auto constructors) and internal (solver/arena types).\n")
print(f"  {'module':<10}{'API*':>7}{'bindable':>10}{'internal':>10}{'ctors':>8}")
t_api = t_bind = t_int = t_ctor = 0
for m in MODULES:
    tree = ast_dump(m)
    if tree is None:
        print(f"  {m:<10}{'(ast-dump failed)':>17}")
        continue
    acc = sorted(set(collect_all(tree, m)))
    ctors = [a for a in acc if a[3] == "CXXConstructorDecl"]
    api = [a for a in acc if a[3] != "CXXConstructorDecl"]  # free fns + methods
    bind = [a for a in api if is_bindable(a[2])]
    internal = [a for a in api if not is_bindable(a[2])]
    print(f"  {m:<10}{len(api):>7}{len(bind):>10}{len(internal):>10}{len(ctors):>8}")
    t_api += len(api); t_bind += len(bind); t_int += len(internal); t_ctor += len(ctors)
    if LIST_MODULE == m:
        for fn, name, qt, k in api:
            tag = "  " if is_bindable(qt) else "x "
            print(f"      {tag}{fn:<26} {name:<30} {qt}")
print(f"  {'-'*45}")
print(f"  {'TOTAL':<10}{t_api:>7}{t_bind:>10}{t_int:>10}{t_ctor:>8}")
print(f"\n  *API     = public free functions + non-ctor methods (the real callable surface)")
print(f"   bindable = API whose params/return are simple/POD/handle types")
print(f"   internal = API mentioning solver/arena/NotNull/Variant/etc. (not worth binding)")
print(f"   ctors    = public constructors (mostly compiler-generated; excluded from API)")
print(f"\n  run with '--list <Module>' to dump that module's API signatures.")
print(f"  run with '--gaps [Module]' for the shim coverage gap map.")

if GAPS:
    print("\n" + "=" * 66)
    print("  SHIM COVERAGE GAP MAP")
    print("  For each public class/struct & free function in the upstream module,")
    print("  is its name referenced ANYWHERE in our shim sources?")
    print("  * UNTOUCHED  = the name never appears -> a whole area we did not bind.")
    print("  * touched    = the name appears, but that only means the shim is AWARE")
    print("    of it. A class can be 'touched' (named to dispatch on) yet expose none")
    print("    of its fields -- e.g. the Ast type-node classes have 0 field accessors.")
    print("    So 'touched' is an UPPER bound on coverage, not a guarantee.\n")
    print(f"  {'module':<10}{'classes':>9}{'touched':>9}{'untouched':>11}   {'freefns':>8}{'touched':>9}")
    missing_by_mod = {}
    for m in MODULES:
        tree = ast_dump(m)
        if tree is None:
            print(f"  {m:<10}{'(ast-dump failed)':>20}")
            continue
        records, freefns = set(), set()
        collect_decls(tree, [None], f"{LUAU}/{m}/include", records, freefns)
        toks = shim_tokens(m)
        cls_miss = sorted(c for c in records if c not in toks)
        cls_hit = len(records) - len(cls_miss)
        fn_hit = sum(1 for f in freefns if f in toks)
        missing_by_mod[m] = (cls_miss, sorted(f for f in freefns if f not in toks))
        print(f"  {m:<10}{len(records):>9}{cls_hit:>9}{len(cls_miss):>11}   "
              f"{len(freefns):>8}{fn_hit:>9}")
    dump = GAPS_MODULE or LIST_MODULE
    if dump and dump in missing_by_mod:
        cls_miss, fn_miss = missing_by_mod[dump]
        print(f"\n  --- {dump}: untouched classes ({len(cls_miss)}) ---")
        for c in cls_miss:
            print(f"      {c}")
        if fn_miss:
            print(f"\n  --- {dump}: untouched free functions ({len(fn_miss)}) ---")
            for f in fn_miss:
                print(f"      {f}")
    else:
        print(f"\n  add a module name (e.g. '--gaps Ast') to list that module's untouched classes.")
