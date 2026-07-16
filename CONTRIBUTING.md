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
- Public API in `include/crankl/crankl.h` (C ABI)
- Implementation in `src/` namespaces; use `::crankl_cran_t` inside `namespace crankl`
