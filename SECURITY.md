# Security Policy

## Supported versions

| Version | Support |
|---------|---------|
| 0.5.x   | active  |
| < 0.5   | none    |

## Reporting a vulnerability

Email **security@deepiri.dev** (PGP key on request). Do not open public
issues or PRs describing the vulnerability. Expect acknowledgement within
72 hours and a fix timeline within 7 days for confirmed reports.

## Scope and hardening posture

crankl parses untrusted input in three places: `.crank` archives (mmap-based
reader), `.safetensors` checkpoints, and GGUF models. Relevant guarantees:

- All mmap/offset accesses are bounds-checked against section tables; malformed
  section tables are rejected, not dereferenced.
- Truncated/corrupt files return typed error codes; no partial processing.
- Layer-history stacks are NULL unless explicitly validated (peel cannot read
  metadata as history).
- A randomized robustness harness (`tests/ctest/test_fuzz_parsers`) mutates
  12 000 inputs across all three parsers per CI run, and the full suite runs
  under ASan+UBSan.
- The shared library exports only `crankl_*` symbols; implementation internals
  are not part of any stability contract.

Out of scope at this stage: side-channel hardening of pack quality metrics,
and denial-of-service via pathological-but-valid inputs that exceed declared
resource ceilings (the CLI caps single-file input at 256 MiB of f32 payload).
