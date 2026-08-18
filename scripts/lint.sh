#!/usr/bin/env bash
# Run clang-tidy using compile_commands.json (configure CMake first).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${1:-build}"
COMPILE_DB="${BUILD_DIR}/compile_commands.json"

if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "error: clang-tidy not found (install clang-tidy)" >&2
    exit 1
fi

if [[ ! -f "${COMPILE_DB}" ]]; then
    echo "error: ${COMPILE_DB} missing — configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
    exit 1
fi

# gui/ needs Qt6, so it is only in the compile database when CMake was
# configured with CRANKL_BUILD_GUI=ON. Lint it when it is there and skip it
# when it is not -- clang-tidy hard-errors on any file the database lacks.
LINT_DIRS=(src)
if grep -q '/gui/' "${COMPILE_DB}"; then
    LINT_DIRS+=(gui)
fi

failed=0
count=0
while IFS= read -r f; do
    [[ -z "$f" ]] && continue
    count=$((count + 1))
    if ! clang-tidy -p "${BUILD_DIR}" --quiet "$f"; then
        failed=1
    fi
done < <(find "${LINT_DIRS[@]}" -type f \( -name '*.cpp' -o -name '*.c' \) ! -path '*/build*' | LC_ALL=C sort)

if [[ "${count}" -eq 0 ]]; then
    echo "no source files found"
    exit 0
fi

if [[ "${failed}" -ne 0 ]]; then
    echo "clang-tidy: findings above" >&2
    exit 1
fi

echo "clang-tidy: ok (${count} files)"
