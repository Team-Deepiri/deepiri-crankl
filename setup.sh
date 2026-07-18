#!/usr/bin/env bash
# Crankl development environment setup
set -euo pipefail

cd "$(dirname "$0")"

echo "=== Crankl Setup ==="

# Git Hooks
if [ -d ".git-hooks" ]; then
    git config core.hooksPath .git-hooks
    echo "Git hooks configured (core.hooksPath = .git-hooks)"
else
    echo "No .git-hooks directory found, skipping hooks setup"
fi

# Build
if command -v cmake >/dev/null 2>&1; then
    echo "Configuring build..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    echo "Building..."
    cmake --build build --parallel
    echo "Exporting golden parity headers..."
    python3 scripts/export_golden.py
    echo "Running tests..."
    ctest --test-dir build --output-on-failure
else
    echo "cmake not found, skipping build"
fi

echo ""
echo "=== Setup complete ==="
