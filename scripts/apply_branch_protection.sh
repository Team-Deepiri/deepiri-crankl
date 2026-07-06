#!/usr/bin/env bash
# Apply branch protection matching deepiri-platform (main + dev).
set -euo pipefail

REPO="${1:-Team-Deepiri/deepiri-crankle}"

apply_main() {
  if ! gh api -X PUT "repos/${REPO}/branches/main/protection" \
    --input - <<'EOF'
{
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 1
  },
  "enforce_admins": false,
  "restrictions": null,
  "required_status_checks": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
EOF
  then
    echo "⚠ main branch protection requires GitHub Team/Pro for private repos."
    echo "  deepiri-platform is public; deepiri-crankle is private."
    echo "  Apply rules manually in GitHub Settings → Branches, or make the repo public."
    return 1
  fi
  echo "✓ main protection applied"
}

apply_dev() {
  if ! gh api -X PUT "repos/${REPO}/branches/dev/protection" \
    --input - <<'EOF'
{
  "required_pull_request_reviews": {
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": false,
    "required_approving_review_count": 1,
    "bypass_pull_request_allowances": {
      "users": [],
      "teams": ["support-team"],
      "apps": []
    }
  },
  "enforce_admins": false,
  "restrictions": null,
  "required_status_checks": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
EOF
  then
    echo "⚠ dev branch protection requires GitHub Team/Pro for private repos."
    return 1
  fi
  echo "✓ dev protection applied"
}

apply_main
apply_dev
