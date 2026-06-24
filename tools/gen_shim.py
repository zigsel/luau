#!/usr/bin/env python3
# Generate extern "C" shim (.h/.cpp) + idiomatic Zig wrappers for a C++ class's
# public methods, straight from the clang AST. Handles the REGULAR subset:
# methods over an opaque handle whose params/return are POD / enums / const char*
# / pointers-or-refs to known classes (exposed as opaque handles). Anything
# irregular (by-value structs, std::vector/optional/templates, references to POD)
# is SKIPPED and reported — those still need hand/agent binding.
#
# usage: gen_shim.py <luau-src> <Module> <Class>[,<Class>...] <out-basename>
import json, subprocess, sys, os, tempfile, re

LUAU, MODULE, CLASSES, OUT = sys.argv[1], sys.argv[2], sys.argv[3].split(","), sys.argv[4]
MODS = ["Ast", "Compiler", "Config", "Bytecode", "CodeGen", "Analysis", "Common", "VM"]
INCLUDES = [f"{LUAU}/{m}/include" for m in MODS]

POD = {
    "void": ("void", "void"), "bool": ("int", "bool"),
    "char": ("char", "u8"), "signed char": ("signed char", "i8"), "unsigned char": ("unsigned char", "u8"),
    "short": ("short", "i16"), "unsigned short": ("unsigned short", "u16"),
    "int": ("int", "i32"), "unsigned int": ("unsigned int", "u32"), "unsigned": ("unsigned int", "u32"),
    "long": ("long", "c_long"), "unsigned long": ("unsigned long", "c_ulong"),
    "long long": ("long long", "i64"), "unsigned long long": ("unsigned long long", "u64"),
    "size_t": ("size_t", "usize"), "float": ("float", "f32"), "double": ("double", "f64"),
    "int8_t": ("signed char", "i8"), "uint8_t": ("unsigned char", "u8"),
    "int16_t": ("short", "i16"), "uint16_t": ("unsigned short", "u16"),
    "int32_t": ("int", "i32"), "uint32_t": ("unsigned int", "u32"),
    "int64_t": ("long long", "i64"), "uint64_t": ("unsigned long long", "u64"),
    "const char *": ("const char *", "[*c]const u8"),
}

def ast_dump():
    headers = []
    incdir = f"{LUAU}/{MODULE}/include"
    for root, _, files in os.walk(incdir):
        for f in files:
            if f.endswith((".h", ".hpp")):
                headers.append(os.path.join(root, f))
    src = "".join(f'#include "{h}"\n' for h in sorted(headers))
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", delete=False) as tf:
        tf.write(src); path = tf.name
    cmd = ["clang++", "-std=c++17", "-fsyntax-only", "-ferror-limit=0", "-Xclang", "-ast-dump=json"]
    for i in INCLUDES:
        cmd += ["-I", i]
    cmd.append(path)
    out = subprocess.run(cmd, capture_output=True, text=True).stdout
    os.unlink(path)
    return json.loads(out)

# gather enum names + record (class/struct) names across the TU
enums, records = set(), set()
def scan(n):
    k = n.get("kind", "")
    if k in ("EnumDecl",) and n.get("name"): enums.add(n["name"])
    if k in ("CXXRecordDecl", "ClassTemplateDecl") and n.get("name"): records.add(n["name"])
    for c in n.get("inner", []) or []: scan(c)

def find_class(n, name):
    if n.get("kind") == "CXXRecordDecl" and n.get("name") == name and n.get("inner"):
        return n
    for c in n.get("inner", []) or []:
        r = find_class(c, name)
        if r: return r
    return None

def strip(t):
    return t.replace("const ", "").replace("&", "").replace("  ", " ").strip()

def map_type(t, as_param):
    t = t.strip()
    if t in POD: return POD[t]
    base = strip(t)
    if base in enums: return ("int", "i32")            # enum -> int
    if base in POD: return POD[base]
    # pointer / reference to a known class -> opaque handle
    m = re.match(r"^(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*[*&]$", t)
    if m and m.group(1) in records:
        return (f"Luau_{m.group(1)}*", f"?*c.Luau_{m.group(1)}")
    return None

def main():
    tu = ast_dump()
    scan(tu)
    decls, shim_h, shim_c, zig, skipped = [], [], [], [], []
    used_handles = set()
    for cls in CLASSES:
        node = find_class(tu, cls)
        if not node:
            print(f"  ! class {cls} not found"); continue
        seen = {}
        cur_access = "public" if node.get("tagUsed") == "struct" else "private"
        for m in node.get("inner", []) or []:
            if m.get("kind") == "AccessSpecDecl":
                cur_access = m.get("access", cur_access); continue
            if m.get("kind") != "CXXMethodDecl": continue
            if (m.get("access") or cur_access) != "public": continue
            if m.get("storageClass") == "static": continue   # v1: instance methods only
            name = m.get("name", "")
            if not name or name.startswith("operator") or name.startswith("~"): continue
            qt = (m.get("type") or {}).get("qualType", "")
            ret = qt[:qt.find("(")].strip() if "(" in qt else "void"
            rmap = map_type(ret, False)
            params = [c for c in m.get("inner", []) or [] if c.get("kind") == "ParmVarDecl"]
            pmaps = [map_type((p.get("type") or {}).get("qualType", ""), True) for p in params]
            if rmap is None or any(x is None for x in pmaps):
                skipped.append(f"{cls}::{name}  ({ret} <- {qt})"); continue
            # disambiguate overloads
            seen[name] = seen.get(name, 0) + 1
            sfx = "" if seen[name] == 1 else f"_{seen[name]}"
            fn = f"luau_{cls.lower()}_{name}{sfx}"
            hname = f"Luau_{cls}"; used_handles.add(cls)
            cargs = ", ".join([f"{hname}* self"] + [f"{pmaps[i][0]} a{i}" for i in range(len(params))])
            shim_h.append(f"{rmap[0]} {fn}({cargs});")
            callargs = ", ".join([f"({strip((params[i].get('type') or {}).get('qualType',''))})a{i}" if (params[i].get('type') or {}).get('qualType','') not in POD else f"a{i}" for i in range(len(params))])
            ret_c = "" if rmap[0] == "void" else "return "
            shim_c.append(f"{rmap[0]} {fn}({cargs}) {{ {ret_c}reinterpret_cast<Luau::CodeGen::{cls}*>(self)->{name}({callargs}); }}")
            zargs = ", ".join(["self: *@This()"] + [f"a{i}: {pmaps[i][1]}" for i in range(len(params))])
            zret = "void" if rmap[1] == "void" else rmap[1]
            zcall = ", ".join([f"a{i}" for i in range(len(params))])
            zig.append(f"    pub fn {name}{sfx}({zargs}) {zret} {{ return c.{fn}(@ptrCast(self){', ' if params else ''}{zcall}); }}")
    print(f"  generated {len(shim_h)} method shims for {','.join(CLASSES)}; skipped {len(skipped)}")
    for s in skipped[:12]: print(f"    skip: {s}")
    if len(skipped) > 12: print(f"    ... and {len(skipped)-12} more")
    # NOTE: this v1 prints to stdout for review; it does not write files yet.

if __name__ == "__main__":
    main()
