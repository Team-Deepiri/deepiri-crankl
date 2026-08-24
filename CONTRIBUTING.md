# Contributing to crankl

Thanks for helping build crankl. The rules are few and strict.

## Branch flow

- `dev` is the integration branch; `main` is the release branch.
- Feature work goes on `feat/...` branches and lands in `dev` through a PR.
- PRs must keep CI green: build matrix, ASan+UBSan, clang-tidy, clang-format
  (auto-applied), CodeQL, and the Qt6 GUI smoke test.

## Invariants (do not break these)

1. **Backward compatibility**: public C symbols are append-only. Struct layout
   changes require a format version bump. v1 fixtures from the first release
   still pass — keep it that way.
2. **Export surface**: the shared object exports only `crankl_*` symbols
   (`cmake/crankl.map`). Do not consume implementation internals outside the
   library; tests that need internals compile the TU directly (see
   `tests/CMakeLists.txt`).
3. **No silent failure**: corrupt/truncated inputs return typed errors. Never
   partially process.
4. **Reader safety**: every mmap access is bounds-checked; layer stacks are
   NULL unless validated.
5. **Determinism**: fixed seeds everywhere; benchmark tables in
   `docs/research/PAPER.md` regenerate exactly from `tools/bench_sweep.c`.
6. **Golden parity**: kernel changes that affect numerics need regenerated
   goldens (`python3 scripts/export_golden.py`) committed alongside, and the
   exporter output must stay idempotent with clang-format.

## Verification before pushing

```
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
ctest --test-dir build --output-on-failure
bash scripts/verify_parity.sh
bash scripts/integration_test.sh
```

Sanitizer replica (same flags as CI):

```
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan --parallel && ctest --test-dir build-asan --output-on-failure
```

## Style

C++20 / C11, 4-space indent, K&R braces, 100-column soft limit,
clang-format enforced (`bash scripts/format.sh`). Conventional commits
(`feat:`, `fix:`, `docs:`, `ci:`, ...).

## Reporting issues

Include the exact command, input characteristics (size/format — not weights),
observed vs expected behavior, and the `crankl version` line. For security
issues see `SECURITY.md` — do not open public issues for them.
