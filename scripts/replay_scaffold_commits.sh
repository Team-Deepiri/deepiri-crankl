#!/usr/bin/env bash
# Replay ~80 scaffold commits for deepiri-crankl history.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
echo "replay script: see git history for scaffold commits"
