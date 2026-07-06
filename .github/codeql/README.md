# CodeQL Setup for deepiri-crankle

This folder contains the CodeQL configuration for repository-level security scanning.

## Files

- `.github/workflows/codeql.yml` — when scans run and how GitHub Actions executes CodeQL.
- `.github/codeql/codeql-config.yml` — folders to ignore during analysis.

## Language

Crankle is analyzed as **C/C++** (`cpp` in the CodeQL matrix).

## Maintenance

To exclude another generated folder, add a glob to `paths-ignore` in `codeql-config.yml`.
