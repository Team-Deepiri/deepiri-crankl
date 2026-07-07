#!/usr/bin/env bash
# Sync issue labels from deepiri-platform to this repository.
set -euo pipefail

REPO="${1:-Team-Deepiri/deepiri-crankl}"
SOURCE="${2:-Team-Deepiri/deepiri-platform}"

echo "Syncing labels from ${SOURCE} → ${REPO}"

gh api "repos/${SOURCE}/labels" --paginate --jq '.[] | [.name, .color, .description] | @tsv' |
  while IFS=$'\t' read -r name color description; do
    gh label create "$name" --repo "$REPO" --color "$color" --description "$description" --force
    echo "  ✓ ${name}"
  done

echo "Done."
