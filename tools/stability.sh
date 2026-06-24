#!/bin/sh
# API-stability report for Luau's C++ headers, from git history.
#
# Churn is NOT correctness: this binding pins a vendored Luau version, so it works
# regardless of upstream churn. This report is the *upgrade map* — when you bump
# the pinned Luau version, it tells you which headers' public signatures moved
# (re-check those) and which stayed put (ignore those).
#
# usage: tools/stability.sh [window]   (default window: "18 months ago")
set -eu

WINDOW="${1:-18 months ago}"
REPO="${LUAU_GIT:-/tmp/luau-git}"

if [ ! -d "$REPO/.git" ]; then
  echo "cloning luau-lang/luau (blobless) into $REPO ..."
  git clone --filter=blob:none --quiet https://github.com/luau-lang/luau.git "$REPO"
fi
cd "$REPO"
git fetch --quiet --filter=blob:none origin 2>/dev/null || true

OLD=$(git rev-list -1 --before="$WINDOW" origin/HEAD)
echo "window: $(git log -1 --format=%cs "$OLD")  ->  $(git log -1 --format=%cs origin/HEAD)"
echo

echo "== per-module header churn (commits in window) =="
printf "  %-9s %6s %6s %6s %6s\n" module hdrs quiet stable churny
for m in Ast Compiler Config Bytecode CodeGen Analysis; do
  q=0; s=0; c=0; h=0
  for f in $(git ls-files "$m/include/*.h" "$m/include/*.hpp"); do
    h=$((h + 1))
    n=$(git log --since="$WINDOW" --oneline -- "$f" | wc -l | tr -d ' ')
    if [ "$n" -eq 0 ]; then q=$((q + 1)); elif [ "$n" -le 3 ]; then s=$((s + 1)); else c=$((c + 1)); fi
  done
  printf "  %-9s %6s %6s %6s %6s\n" "$m" "$h" "$q" "$s" "$c"
done
echo "  (quiet = 0 commits, stable = 1-3, churny > 3, over the window)"
echo

echo "== churny headers — re-check these on a Luau bump =="
for m in Ast Compiler Config Bytecode CodeGen Analysis; do
  for f in $(git ls-files "$m/include/*.h" "$m/include/*.hpp"); do
    n=$(git log --since="$WINDOW" --oneline -- "$f" | wc -l | tr -d ' ')
    [ "$n" -gt 3 ] && echo "$n $f"
  done
done | sort -rn | sed 's#/include/Luau/#/#; s#/include/#/#' | awk '{printf "  %3s  %s\n", $1, $2}'
