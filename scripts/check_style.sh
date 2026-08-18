#!/usr/bin/env bash
# Fail if any C/C++ source is not clang-format clean.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "error: clang-format not found (install clang-format)" >&2
    exit 1
fi

count=0
while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    clang-format --dry-run --Werror "$f"
    count=$((count + 1))
done < <(
    find include src tests gui -type f \
        \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
        ! -path '*/build/*' | LC_ALL=C sort
)

echo "clang-format: ok (${count} files)"
