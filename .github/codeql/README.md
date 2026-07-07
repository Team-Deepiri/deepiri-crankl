# CodeQL Setup for deepiri-crankl

This folder contains the CodeQL configuration for repository-level security scanning.

## Files

- `.github/workflows/codeql.yml` — when scans run and how GitHub Actions executes CodeQL.
- `.github/codeql/codeql-config.yml` — folders to ignore during analysis.

## Language

Crankl is analyzed as **C/C++** (`cpp` in the CodeQL matrix). The workflow runs `autobuild` so CodeQL traces the CMake compile.

## Private repository note

`deepiri-crankl` is private; uploading CodeQL results requires **GitHub Advanced Security** on the repo or org. Until GHAS is enabled, the analyze step uses `continue-on-error` so CI stays green while scans still run. `deepiri-platform` is public and uploads SARIF without GHAS.

## Maintenance

To exclude another generated folder, add a glob to `paths-ignore` in `codeql-config.yml`.
