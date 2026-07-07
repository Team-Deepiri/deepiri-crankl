# CRAN format v1

Crankl Archive (`.cran`) — mmap-friendly little-endian container.

## Header (128 bytes)

| Offset | Size | Field |
|--------|------|-------|
| 0 | 6 | magic `CRAN\x01` |
| 6 | 2 | version u16 |
| 8 | 8 | n_slots u64 |
| 16 | 4 | depth_max u32 |
| 20 | 4 | gamma f32 |
| 24 | 4 | flags u32 |
| 28 | 8 | checksum xxhash64 of payload |
| 36 | 92 | reserved |

## Payload

`n_slots` × `uint64` crank words, little-endian.

## Checksum

`xxhash64(payload, len, seed=0)` using Crankl vendored hash.
