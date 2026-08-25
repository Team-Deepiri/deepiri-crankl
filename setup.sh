#!/usr/bin/env bash
# crankl one-shot setup: install dependencies, configure, build, test.
#
# Usage:
#   ./setup.sh                    # core library + CLI + tests
#   ./setup.sh --with-gui         # also build the Qt6 desktop GUI
#   ./setup.sh --with-fuzzers     # also build libFuzzer targets (clang)
#   ./setup.sh --install [PREFIX] # after tests, install into PREFIX (default /usr/local)
#   ./setup.sh --no-deps          # skip system package installation
#
# Supported platforms: Linux (apt/dnf/pacman) and macOS (brew).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WITH_GUI=0
WITH_FUZZERS=0
INSTALL_PREFIX=""
SKIP_DEPS=0
BUILD_DIR="build"
CC_BIN="${CC:-}"
CXX_BIN="${CXX:-}"

while [ $# -gt 0 ]; do
    case "$1" in
        --with-gui) WITH_GUI=1 ;;
        --with-fuzzers) WITH_FUZZERS=1 ;;
        --install)
            INSTALL_PREFIX="${2:-/usr/local}"
            shift
            ;;
        --no-deps) SKIP_DEPS=1 ;;
        -h|--help)
            sed -n '2,12p' "$0"
            exit 0
            ;;
        *)
            echo "setup.sh: unknown option '$1' (see --help)" >&2
            exit 2
            ;;
    esac
    shift
done

have() { command -v "$1" >/dev/null 2>&1; }

install_pkgs() {
    if [ "$SKIP_DEPS" -eq 1 ]; then
        echo "== skipping dependency installation =="
        return 0
    fi
    echo "== installing build dependencies =="
    local pkgs_core pkgs_gui pkgs_fuzz
    if have apt-get; then
        pkgs_core="build-essential cmake ninja-build python3 clang"
        pkgs_gui="qt6-base-dev"
        [ "$WITH_GUI" -eq 1 ] || pkgs_gui=""
        sudo apt-get update -qq
        # shellcheck disable=SC2086
        sudo apt-get install -y $pkgs_core $pkgs_gui
    elif have dnf; then
        pkgs_core="gcc-c++ cmake ninja-build python3 clang"
        pkgs_gui="qt6-qtbase-devel"
        [ "$WITH_GUI" -eq 1 ] || pkgs_gui=""
        # shellcheck disable=SC2086
        sudo dnf install -y $pkgs_core $pkgs_gui
    elif have pacman; then
        pkgs_core="base-devel cmake ninja python clang"
        pkgs_gui="qt6-base"
        [ "$WITH_GUI" -eq 1 ] || pkgs_gui=""
        # shellcheck disable=SC2086
        sudo pacman -S --needed --noconfirm $pkgs_core $pkgs_gui
    elif have brew; then
        pkgs_core="cmake ninja python3 llvm"
        brew install $pkgs_core
        if [ "$WITH_GUI" -eq 1 ]; then
            brew install qt
        fi
    else
        cat >&2 <<'EOF'
setup.sh: no known package manager found (apt-get/dnf/pacman/brew).
Install these tools yourself and re-run with --no-deps:
  a C++17 compiler, cmake >= 3.16, ninja (optional), python3
EOF
        exit 3
    fi
}

pick_compiler() {
    # Prefer gcc/g++ everywhere except macOS where apple clang is the default;
    # fuzzers force clang.
    if [ "$WITH_FUZZERS" -eq 1 ]; then
        CC_BIN="${CC_BIN:-clang}"
        CXX_BIN="${CXX_BIN:-clang++}"
        return 0
    fi
    if [ "$(uname -s)" != "Darwin" ]; then
        have g++ && CXX_BIN="${CXX_BIN:-g++}"
        have gcc && CC_BIN="${CC_BIN:-gcc}"
    fi
}

configure_and_build() {
    echo "== configuring ($BUILD_DIR) =="
    local generator="-G Ninja"
    have ninja || generator=""
    local flags=(-DCMAKE_BUILD_TYPE=Release)
    [ -n "$CC_BIN" ] && flags+=("-DCMAKE_C_COMPILER=$CC_BIN")
    [ -n "$CXX_BIN" ] && flags+=("-DCMAKE_CXX_COMPILER=$CXX_BIN")
    [ "$WITH_GUI" -eq 1 ] && flags+=("-DCRANKL_BUILD_GUI=ON")
    [ "$WITH_FUZZERS" -eq 1 ] && flags+=("-DCRANKL_BUILD_FUZZERS=ON")
    # shellcheck disable=SC2086
    cmake -B "$BUILD_DIR" $generator "${flags[@]}"

    echo "== building =="
    cmake --build "$BUILD_DIR" --parallel

    echo "== testing =="
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

do_install() {
    echo "== installing into $INSTALL_PREFIX =="
    cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX"
    echo "installed: $INSTALL_PREFIX/bin/crankl (+ headers, lib, pkg-config, man page)"
}

install_pkgs
pick_compiler
configure_and_build

if [ "$WITH_FUZZERS" -eq 1 ]; then
    echo "== fuzzer smoke campaign (10s per parser) =="
    local_corpus="$ROOT/build/fuzz-corpus"
    mkdir -p "$local_corpus"
    for t in cran safetensors gguf; do
        "$BUILD_DIR/tests/fuzz_$t" -max_total_time=10 "$local_corpus/$t" \
            >"$local_corpus/$t.log" 2>&1 || {
            echo "fuzzer $t reported a crash — see $local_corpus/$t.log" >&2
            exit 4
        }
        tail -2 "$local_corpus/$t.log"
    done
fi

[ -n "$INSTALL_PREFIX" ] && do_install

echo "setup complete."
echo "  CLI:            ./$BUILD_DIR/crankl"
echo "  quick start:    ./$BUILD_DIR/crankl pack --input model.f32 -o model.crank"
[ "$WITH_GUI" -eq 1 ] && echo "  GUI:            ./$BUILD_DIR/gui/crankl-gui"
exit 0
