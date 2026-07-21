# Contributing to Crankl

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Commit style

Use conventional prefixes: `core:`, `cli:`, `io:`, `pack:`, `test:`, `docs:`, `ci:`, `build:`.

## Math changes

Update the matching notebook under `notebooks/` and refresh golden files:

```bash
python3 scripts/export_golden.py
bash scripts/verify_parity.sh
```

## C++ conventions

- C++20, `-Wall -Wextra`
- Public API in `include/crankl/*.h` (C ABI)
- Implementation in `src/` namespaces; C bindings in `src/c_bindings/bind_*.cpp`
- Private headers in `src/internal_headers/`

## Style and lint

Format and lint before opening a PR:

```bash
# Apply clang-format (.clang-format)
bash scripts/format.sh

# Verify formatting (CI style job)
bash scripts/check_style.sh

# clang-tidy (needs compile_commands.json)
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCRANKL_ENABLE_SIMD=OFF
bash scripts/lint.sh build
```

On pull requests from this repository, CI auto-applies `clang-format` and pushes a
`style: apply clang-format` commit when needed. Fork PRs must format locally.

