# CRANK archive format and v3 implementation plan

Crankl archives use the `.crank` extension and store compressed model-weight tiles. Every
8-byte **slot** is one crank word representing one 8x8 weight tile. The archive can also carry
two independent kinds of optional data:

- **Layer stacks** are snapshots of all slots after optimization steps. They are weight data and
  allow `peel` to restore an older state.
- **Metadata** is provenance: a model name and source hash encoded as JSON. It describes where the
  archive came from, but cannot restore weights.

These concepts must stay separate. A metadata footer is a label; a stack layer is a full checkpoint.

## Terminology

- **Current slots**: the latest compressed weights. These are always present.
- **Layer**: one complete snapshot containing `n_slots` crank words.
- **Layer stack**: zero or more layers laid out oldest to newest.
- **Peel**: replace current slots with an older layer from the stack.
- **Metadata/provenance**: descriptive data such as source model and source hash.
- **Tail**: all bytes after the current slots.

## Current on-disk formats (versions 1 and 2)

All integer and floating-point fields are currently written in the host's little-endian
representation. Supported files therefore assume a little-endian host.

### Current header: 124 bytes in the implementation

`CranHeaderDisk` is packed and currently contains 88 reserved bytes. Despite the old documentation
and `CRAN_HEADER_SIZE` constant saying 128, `sizeof(CranHeaderDisk)` is therefore **124 bytes**:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 6 | magic `CRANK\x01`; legacy readers also accept `CRAN\x01` |
| 6 | 2 | format version |
| 8 | 8 | `n_slots` |
| 16 | 4 | `depth_max` |
| 20 | 4 | holonomy `gamma` |
| 24 | 4 | flags; bit 0 currently means metadata is present |
| 28 | 8 | xxhash64 of the entire payload |
| 36 | 88 | reserved |

Do not silently change the existing structure to 128 bytes: old files place their first slot at
offset 124. A corrected 128-byte header requires a new format version and version-aware payload
offsets.

### Version 1: slots only

```text
[124-byte header]
[n_slots * uint64 current slots]
```

### Version 2A: slots plus layer stacks

Written by `write_cran()` when `layer_stacks != nullptr`:

```text
[124-byte header; version=2; metadata flag clear]
[n_slots * uint64 current slots]
[uint32 n_stack_layers]
[n_stack_layers * n_slots * uint64 stack words]
```

Stack indexing is:

```text
layer_stacks[layer_index * n_slots + slot_index]
```

The CLI currently records a full layer after every `turn` or `finetune` step. The last stored layer
normally duplicates the current slots. For two slots and two optimization steps, the payload is:

```text
current:  B0 B1
count:    2
layer 0:  A0 A1
layer 1:  B0 B1
```

Peeling one layer should restore `A0 A1`.

### Version 2B: slots plus metadata

Written by `write_cran_with_metadata()`:

```text
[124-byte header; version=2; flags bit 0 set]
[n_slots * uint64 current slots]
[uint32 META magic = 0x4D455441]
[uint32 JSON byte length]
[JSON bytes, no terminating NUL]
```

Current JSON is:

```json
{"model":"MODEL_NAME","hash":"SOURCE_HASH"}
```

### Current design defect

Version 2 has two incompatible tail interpretations. When metadata flag bit 0 is set, the reader
interprets the entire tail as metadata and returns before looking for stacks. When it is clear, the
reader may interpret the tail as stacks. Therefore one file cannot safely carry both.

There is a second defect: when no validated stack exists, `read_cran()` sets `cran.layers` to the
address immediately after the slots instead of `nullptr`. `cmd_peel()` uses pointer inequality to
decide whether a stack exists, so metadata bytes or the end of the mapping can be mistaken for
history.

## Proposed version 3

Version 3 should make each optional section explicit and permit both sections in one archive.

### Header

Use a real 128-byte packed header:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 6 | `CRANK\x01` |
| 6 | 2 | version = 3 |
| 8 | 8 | `n_slots` |
| 16 | 4 | number of stack layers; zero means no history |
| 20 | 4 | holonomy `gamma` |
| 24 | 4 | section flags |
| 28 | 8 | xxhash64 of bytes from offset 128 to EOF |
| 36 | 92 | reserved and zero-filled |

Recommended flags:

```cpp
CRANK_FLAG_METADATA = 1u << 0;
CRANK_FLAG_STACKS   = 1u << 1;
```

For v3, the field currently exposed as `depth_max` should mean the **actual number of serialized
stack layers**, not merely the number of optimization steps requested. A future cleanup may rename
it to `n_stack_layers`; while the public name remains `depth_max`, document this exact meaning.

### Payload

Use a deterministic section order:

```text
[128-byte v3 header]
[n_slots * uint64 current slots]

if CRANK_FLAG_STACKS:
    [n_stack_layers * n_slots * uint64 stack words]

if CRANK_FLAG_METADATA:
    [uint32 META magic]
    [uint32 JSON byte length]
    [JSON bytes]
```

The layer count is already in the v3 header, so v3 does not need the extra four-byte count used by
v2. Keeping the count in exactly one place avoids disagreement between header and payload.

Example with three slots, two layers, and metadata:

```text
header:
    version = 3
    n_slots = 3
    depth_max = 2
    flags = STACKS | METADATA

payload:
    current slots: C0 C1 C2
    layer 0:       A0 A1 A2
    layer 1:       B0 B1 B2
    metadata:      META, json_len, {"model":"adapter-a","hash":"abc123"}
```

## Reader implementation sketch

Implement parsing in this order:

1. Read enough bytes to inspect magic and version.
2. Select the header size from the version:
   - versions 1-2: 124 bytes
   - version 3: 128 bytes
3. Validate `n_slots` before multiplying.
4. Compute `slots_bytes = n_slots * 8` with overflow checks.
5. Verify the file contains the header and all current slots.
6. For v1/v2, preserve the existing parsing rules for backward compatibility.
7. For v3:
   - Set `out->layers = nullptr` initially.
   - If `STACKS` is set, validate `depth_max > 0`, calculate
     `depth_max * n_slots * 8` with overflow checks, and point `layers` to that section.
   - Advance the cursor past the validated stack bytes.
   - If `METADATA` is set, validate the META magic, JSON length limit, and exact bounds.
   - Reject unrecognized flag bits and malformed or unexplained trailing bytes.
8. Verify the checksum over the complete payload.
9. Populate the public view only after every validation succeeds.

Never infer stack presence from pointer inequality, version alone, or the existence of tail bytes.

Pseudocode:

```cpp
cursor = payload + slots_bytes;
remaining = payload_len - slots_bytes;
layers = nullptr;

if (flags & CRANK_FLAG_STACKS) {
    stack_bytes = checked_mul(depth_max, n_slots, sizeof(uint64_t));
    require(remaining >= stack_bytes);
    layers = reinterpret_cast<const uint64_t *>(cursor);
    cursor += stack_bytes;
    remaining -= stack_bytes;
}

if (flags & CRANK_FLAG_METADATA) {
    validate_metadata_section(cursor, remaining);
    cursor += metadata_section_size;
    remaining -= metadata_section_size;
}

require(remaining == 0);
```

## Writer implementation sketch

Create one internal writer that accepts all optional sections. The old public functions can remain
as compatibility wrappers:

```cpp
struct CranWriteOptions {
    const uint64_t *layer_stacks = nullptr;
    uint32_t n_stack_layers = 0;
    const CranMetadata *metadata = nullptr;
};

int write_cran_v3(const char *path,
                  const crankl_cran_header_t *header,
                  const uint64_t *slots,
                  const CranWriteOptions &options);
```

Writer steps:

1. Validate all required pointers and limits before opening the output file.
2. Reject inconsistent options:
   - stack pointer with zero layer count
   - nonzero layer count with null stack pointer
3. Compute all section sizes with overflow checks.
4. Set flags from the sections actually supplied; do not trust caller flags for section presence.
5. Serialize slots, stacks, then metadata.
6. Hash that complete payload.
7. Write a zero-initialized 128-byte v3 header followed by the payload.
8. Check every `fwrite` and `fclose` result; remove a partially written file on failure if practical.

Compatibility wrappers:

- `crankl_cran_write()` calls the v3 writer with optional stacks and no metadata.
- `crankl_cran_write_with_metadata()` calls it with metadata and no stacks.
- Add a new API, such as `crankl_cran_write_full()`, for callers that need both.

The existing unused `depths` argument should either be removed in the new API or explicitly assigned
a format meaning. Do not serialize an undocumented array.

## Public API changes

The mapped archive view needs an unambiguous stack count:

```cpp
typedef struct crankl_cran {
    void *mmap_base;
    size_t mmap_size;
    crankl_cran_header_t header;
    const uint64_t *slots;
    const uint64_t *layers; // nullptr when no validated stack section exists
    uint32_t n_stack_layers;
} crankl_cran_t;
```

Recommended full-writer API:

```cpp
int crankl_cran_write_full(const char *path,
                           const crankl_cran_header_t *header,
                           const uint64_t *slots,
                           const uint64_t *layer_stacks,
                           uint32_t n_stack_layers,
                           const crankl_cran_metadata_t *metadata);
```

This project is still pre-1.0, so extending the public struct is reasonable, but update the shared
library ABI version when doing so.

## CLI changes

- `turn` and `finetune`: write stack history and preserve metadata read from the input archive.
- `pipeline`: write metadata and the stack history created by its optimization phase.
- `peel`: use `n_stack_layers > 0 && layers != nullptr`, never pointer inequality.
- `peel`: after restoring a prior layer, write only the retained history layers and update the count.
  Do not discard all history unless that behavior is explicitly requested.
- `bind`: define provenance and history semantics before combining archives. The safest initial
  behavior is to write no history and metadata recording both source hashes.
- `inspect`: report `has_stacks` and `n_stack_layers` alongside metadata information.

## Required tests

Add tests for:

1. Slots-only v3 roundtrip.
2. Slots plus stacks, including `peel 1` and peeling to the oldest layer.
3. Slots plus metadata.
4. Slots plus both stacks and metadata.
5. Metadata-only archives expose `layers == nullptr`.
6. Header count and stack data size disagreement.
7. Truncated slots, stack section, metadata header, and JSON body.
8. Unknown flags.
9. Checksum mismatch in each section.
10. Overflowing slot/layer dimensions.
11. Backward-compatible reading of existing v1 and both v2 layouts.
12. `turn -> save -> reopen -> peel` end to end.
13. `finetune -> save -> reopen -> peel` while preserving metadata.

## Separate mmap lifetime problem

The current reader stores one process-global mapping. Opening a second archive unmaps the first,
even though its `crankl_cran_t` still contains pointers. This is separate from the format redesign
but should be fixed in the same I/O hardening work:

- Store and close each mapping through the supplied `crankl_cran_t`.
- Do not use `g_mmap_base`, `g_mmap_size`, or `g_mmap_fd`.
- Add a per-handle file descriptor internally, or close the descriptor immediately after a
  successful `mmap` where the operating system permits it.
- Test two simultaneously open archives and close them in both orders.

## Definition of done

The format work is complete when one archive can simultaneously:

- expose current slots,
- expose a validated, countable history stack,
- expose provenance metadata,
- survive close/reopen with the same data,
- peel safely without reading metadata or out-of-bounds bytes,
- reject corrupt dimensions and sections,
- and remain readable alongside legacy v1/v2 fixtures.
