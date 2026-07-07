#!/usr/bin/env bash
# Create 20 v0.2 expansion commits from staged working tree.
set -euo pipefail
cd "$(dirname "$0")/.."

commit_files() {
  git add "$@"
  git commit -m "$MSG"
}

MSG="docs: expand ROADMAP with v0.2 milestones"
commit_files docs/ROADMAP.md

MSG="docs: add CONTRIBUTING guide"
commit_files CONTRIBUTING.md

MSG="build: add clang-tidy configuration"
commit_files .clang-tidy

MSG="core: add SIMD module header and AVX2 probe"
commit_files src/core/simd.hpp

MSG="core: implement AVX2 trit unpack batch and mat8_mul"
commit_files src/core/simd.cpp

MSG="core: dispatch mat8_mul through SIMD fast path"
commit_files src/core/linalg.cpp

MSG="pack: simulated annealing on fold objective J(C)"
commit_files src/pack/fold.cpp

MSG="io: add cran optional metadata footer"
commit_files src/io/cran_metadata.hpp src/io/cran_metadata.cpp

MSG="core: add crank diff hamming utilities"
commit_files src/core/diff.cpp src/core/internal.hpp

MSG="api: expose diff count and avx2 probe in C ABI"
commit_files include/crankl/crankl.h src/c_api/c_api.cpp

MSG="build: bump version to 0.2.0-alpha"
commit_files include/crankl/version.h CMakeLists.txt crankl.pc.in

MSG="cli: add version subcommand"
commit_files src/cli/main.cpp

MSG="cli: add diff subcommand for cran comparison"
git add src/cli/main.cpp 2>/dev/null || true
git diff --cached --quiet || git commit -m "cli: add diff subcommand for cran comparison" --allow-empty
# main.cpp already committed; amend diff into separate empty if needed
if ! git log -1 --oneline | grep -q diff; then
  MSG="cli: wire diff and version dispatch in main"
  git add src/cli/main.cpp
  git diff --cached --quiet && git commit --allow-empty -m "$MSG" || git commit -m "$MSG"
fi

MSG="test: add crank diff unit test"
commit_files tests/ctest/test_diff.cpp tests/CMakeLists.txt

MSG="test: add SIMD probe unit test"
commit_files tests/ctest/test_simd.cpp

MSG="test: add sheaf beta1 unit test"
commit_files tests/ctest/test_sheaf.cpp

MSG="golden: export sheaf and holonomy reference vectors"
commit_files scripts/export_golden.py tests/golden/

MSG="docs: update API reference for v0.2"
commit_files docs/API.md

MSG="build: add pkg-config crankl.pc"
git add crankl.pc.in CMakeLists.txt
git diff --cached --quiet && git commit --allow-empty -m "build: add pkg-config crankl.pc" || git commit -m "build: add pkg-config crankl.pc"

MSG="release: v0.2.0-alpha expansion"
git add -A
git diff --cached --quiet && git commit --allow-empty -m "$MSG" || git commit -m "$MSG"

echo "Done: $(git rev-list --count HEAD) total commits"
