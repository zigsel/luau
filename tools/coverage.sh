#!/bin/sh
# Coverage report, in two honest halves.
#
#   A. Binding gate (exact):  every symbol translate-c sees — the genuine C API
#      plus our own extern "C" shims — must be wrapped as `c.<name>` in src/.
#      This is what gates the build.
#   B. C++ surface (informational): how much of Luau's *internal* C++ API we have
#      shimmed vs. its full (heuristic) size. This is NOT complete and NOT gated;
#      it exists so the report never implies the C++ side is exhaustive.
#
# usage: coverage.sh <translated.zig> <src-dir> <shim.h> [cpp-include-dir ...]
set -eu

TRANSLATED="$1"
SRCDIR="$2"
SHIM_DIR="$3"
shift 3 # remaining args: Luau C++ public include dirs

# symbols intentionally not wrapped (no callable Zig equivalent):
#   lua_pushvfstring — takes a C va_list (superseded by pushFString/raiseError)
ALLOW="lua_pushvfstring"

# ---- A. binding gate ---------------------------------------------------------

names=$(grep -oE 'pub extern fn [A-Za-z0-9_]+' "$TRANSLATED" | awk '{print $4}' | sort -u)
# our shimmed C++ entry points: luau_* functions declared across shim/**/*.h
shim_fns=$(find "$SHIM_DIR" -name '*.h' -exec grep -hoE 'luau_[A-Za-z0-9_]+\(' {} + | tr -d '(' | sort -u)

total=0; bound=0; allow=0; missing=""
for n in $names; do
  total=$((total + 1))
  if grep -rqE "c\.${n}\b" "$SRCDIR"; then
    bound=$((bound + 1))
  elif echo " $ALLOW " | grep -q " $n "; then
    allow=$((allow + 1))
  else
    missing="$missing $n"
  fi
done

echo "== A. binding gate (C API + our shims) =="
echo "   $((bound + allow))/$total declared symbols wrapped ($allow allowlisted)"
if [ -n "$missing" ]; then
  echo "   MISSING:"
  for m in $missing; do echo "     - $m"; done
  exit 1
fi
echo "   OK — every declared binding is wrapped."

# ---- B. C++ surface (informational) -----------------------------------------

# our shimmed C++ entry points = extern "C" functions we declared in shim.h
shim_count=$(printf '%s\n' "$shim_fns" | grep -c . || true)

echo
echo "== B. Luau C++ surface (informational, NOT gated) =="
printf "   %-10s %8s %8s %9s\n" module headers classes "decls(~)"

th=0; tc=0; td=0
for dir in "$@"; do
  [ -d "$dir" ] || continue
  mod=$(basename "$(dirname "$dir")") # .../Ast/include -> Ast
  hdrs=$(find "$dir" \( -name '*.h' -o -name '*.hpp' \) 2>/dev/null)
  hc=$(printf '%s\n' "$hdrs" | grep -c . || true)
  cc=$(cat $hdrs 2>/dev/null | grep -cE '^[[:space:]]*(class|struct)[[:space:]]+[A-Za-z]' || true)
  dc=$(cat $hdrs 2>/dev/null | grep -cE '\)[[:space:]]*(const)?[[:space:]]*(noexcept)?[[:space:]]*;[[:space:]]*$' || true)
  printf "   %-10s %8s %8s %9s\n" "$mod" "$hc" "$cc" "$dc"
  th=$((th + hc)); tc=$((tc + cc)); td=$((td + dc))
done
printf "   %-10s %8s %8s %9s\n" "TOTAL" "$th" "$tc" "$td"
echo
echo "   shimmed C++ entry points: $shim_count"
echo "   note: '$td' above is a rough grep count. Run 'zig build api' for the accurate"
echo "         clang-parsed public surface (~3343 functions/methods; ~2770 bindable)."
echo "         These C++ libraries are internal & unstable, bound selectively against a"
echo "         pinned Luau version. 'A' is the real guarantee; 'B' is best-effort breadth."
