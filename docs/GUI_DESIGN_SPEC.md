# Crankl Qt GUI Design Specification

## 1. Purpose

This document specifies a desktop GUI for Crankl.

The GUI is a **Qt 6 Widgets application written in C++20**. It links
directly to `libcrankl` through the public API in `include/crankl/`. Weight
files live on the user's machine.

The GUI has four goals:

1. Make it obvious which input file each operation requires.
2. Preview the effect of an operation before writing a new artifact.
3. Visualize compressed weights, reconstruction quality, archive health, and
   differences between artifacts.
4. Explain which results are exact and which are current Crankl approximations.

## 2. Supported inputs and outputs

### 2.1 Raw float32 weights

Accepted extensions are `.f32` and `.bin`.
The contents are to be interpreted as a contiguous array of little-endian IEEE-754
32-bit floats.

The import panel must show:

- file path and byte size
- endianness selector, fixed to **Little-endian (required by Crankl)** in the
  first release
- float count: `file_size / 4`
- whether trailing bytes exist when `file_size % 4 != 0`
- inferred slot count: `ceil(float_count / 64)`
- padding count for the final partial block
- an optional user-entered logical shape used only for display and validation

Logical shape does not change Crankl's flat tiling. The UI must say:

> Crankl reads this file as a flat float32 array and divides it into consecutive
> 64-value blocks. A shape entered here documents the source tensor; it does
> not change packing order.

Reject empty files, files with a non-multiple-of-four size, unreadable files,
and files above the current raw-float limit of 256 MiB.

### 2.2 Safetensors weights

Accepted extension: `.safetensors`.

The user flow is:

1. Select or drop a safetensors file.
2. The GUI scans its header.
3. A searchable tensor table appears with columns **Name**, **Dtype**,
   **Shape**, **Elements**, and **Bytes**.
4. Only F32 tensors are selectable in the first release.
5. The user selects exactly one tensor.
6. The GUI shows its inferred number of 8x8 blocks and final-block padding.

Current backend support can read one named F32 tensor through the internal
`crankl::io::read_safetensors_f32()` function, but the public C API cannot list
tensors. Before this screen is implemented, add:

```c
int crankl_safetensors_list(const char *path,
                            crankl_tensor_info_t *items,
                            size_t capacity,
                            size_t *count);
int crankl_safetensors_read_f32(const char *path,
                                const char *tensor_name,
                                float *out,
                                size_t capacity,
                                size_t *count);
```

The listing operation must parse only the header. It must enforce the existing
16 MiB header and 512 MiB tensor-slice limits. Unsupported dtypes remain visible
but disabled with the reason **Only F32 tensors are currently supported**.

### 2.3 Crank archives

Accepted extension: `.crank`.

Immediately after selection, the application calls:

- `crankl_cran_read()`;
- `crankl_cran_verify()`;
- `crankl_cran_compute_metrics()`;
- `crankl_cran_read_metadata()`;
- then copies all data needed by the UI and calls `crankl_cran_close()`.

The archive card displays:

- filename and full path;
- format version when exposed by the backend;
- file size;
- verification status;
- current slot count;
- depth range;
- gamma;
- flags;
- metadata model and source hash, when present;
- whether validated rollback history is present;
- number of history layers, when known.

The current `crankl_cran_t` does not expose format version or a trustworthy
history count. The v3 archive work must add those fields before the GUI enables
history-based peeling. Until then, metadata-only archives must show **Rollback
unavailable: archive history cannot be validated safely**.

### 2.4 Targets and calibration vectors

- A reconstruction target is a raw little-endian float32 file. Ideally it has
  `n_slots * 64` floats.
- Calibration X and Y are raw float32 vectors. Both are required together.
- X and Y may differ in length in the CLI, which silently uses the shorter
  length. The GUI must instead show both lengths and require explicit
  confirmation before truncating to `min(x_count, y_count)`.
- A holonomy input is one raw float32 vector. Its output has the same number of
  floats.

### 2.5 Output files

The GUI can write:

- `.crank` archives;
- reconstructed `.f32` files;
- holonomy result `.f32` files;
- pipeline `.json` manifests;
- comparison and inspection reports as `.json`;
- optional chart exports as `.png` or `.svg`.

Report export is a GUI feature, not an archive-format feature. Use Qt JSON APIs
to serialize typed result models.

## 3. Application Layout

Use `QMainWindow` with the following regions:

### 3.1 Top toolbar

Left to right:

- **Open**: opens a weight or archive file;
- **New operation**: opens an action menu;
- breadcrumb showing the current workspace and operation;
- global job indicator;
- **Help** button.

### 3.2 Left navigation

Use a fixed-width navigation list with these groups:

**Workspace**

- Home
- Inspect
- Compare

**Process**

- Pack
- Optimize
- History
- Bind
- Forward
- Unpack
- Pipeline

**Developer**

- Advanced Math Lab
- Jobs
- Settings
- Help

`QStackedWidget` hosts one page per destination. Navigation state persists with
`QSettings`.

### 3.3 Main content convention

Every operation page uses the same three-column mental model:

1. **Inputs and parameters** on the left;
2. **Preview and visuals** in the center;
3. **Output and run summary** on the right or in a bottom action bar.

On narrow windows, the right panel moves below the center panel.

## 4. Home and workspace

### 4.1 Empty state

Show:

- a large drop target: **Drop weights, safetensors, or a .crank archive**;
- **Open weights**;
- **Open archive**;
- quick actions for **Pack weights**, **Inspect archive**, and **Compare two
  archives**;
- a concise explanation of the flow:
  `Weights -> Pack -> Optimize -> Inspect/Compare -> Export or Forward`.

### 4.2 Selected raw weights

Show a source summary and recommended next action **Pack into .crank**. Also
offer **Use as target**, **Use as calibration vector**, and **Preview values**.

The preview contains:

- first 64 values as an 8x8 heatmap;
- min, max, mean, absolute mean;
- total floats and tiles;
- a tile index spin box and slider.

### 4.3 Selected archive

Show the Archive Overview specified below and action buttons:

- **Inspect details**
- **Optimize**
- **Compare**
- **Peel history**
- **Bind**
- **Run forward**
- **Unpack**

Buttons that are unsafe or inapplicable are disabled with a tooltip explaining
why.

## 5. Shared visual components

### 5.1 Archive health header

At the top of archive-based pages, display:

- green **Verified** or red **Invalid** status;
- `n_slots`;
- depth range;
- file size;
- gamma;
- metadata/history badges.

Backend calls:

- `crankl_cran_read()`
- `crankl_cran_verify()`
- `crankl_cran_read_metadata()`
- `crankl_cran_compute_metrics()`

### 5.2 Metrics panel

Display:

- slots;
- depth min/max;
- scalar mean;
- scalar absolute mean;
- trit density;
- trit entropy;
- Clifford energy;
- beta1 proxy.

Use value rows and three restrained plots:

1. a ternary density bar for zero versus non-zero trits;
2. a depth range plot;
3. a scalar/energy summary plot.

Each metric has a tooltip with its definition. Beta1 always includes the
**proxy** suffix.

Backend calls:

- `crankl_cran_compute_metrics()` for a mapped archive;
- `crankl_compute_archive_metrics()` for copied or newly generated slots;
- `crankl_sheaf_beta1_proxy()` for explicit recalculation in the Math Lab.

### 5.3 Slot browser

The slot browser consists of:

- slot index spin box;
- previous/next buttons;
- search by hexadecimal word;
- 64-bit hexadecimal crank word;
- depth and flags;
- scalar, vector, bivector, and pseudoscalar coefficients;
- an 8x8 matrix heatmap;
- a numerical 8x8 table toggle.

Backend calls:

- `crankl_crank_to_multivector()`
- `crankl_decrank_matrix()`

### 5.4 Source versus reconstruction viewer

When source/target floats are available, show three synchronized 8x8 heatmaps:

- **Source block**
- **Reconstructed block**
- **Error: reconstructed - source**

Below them show:

- per-block Frobenius loss;
- running total loss;
- maximum absolute error;
- previous/next block controls;
- a block-loss sparkline or line plot across all slots.

Backend calls:

- `crankl_decrank_matrix()`
- `crankl_decrank_frobenius_loss()`

### 5.5 Help page

Every feature page has a **What does this do?** link to the help page. It describes the
operation in plain language, the mathematical approximation being used, and
what is written.

## 6. Pack weights

### 6.1 Layout

Use a three-step wizard.

**Step 1: Select weights**

- drop target;
- **Browse** button;
- format selector auto-detected as Raw F32 or Safetensors;
- safetensors tensor table when needed;
- raw-file interpretation panel;
- input preview heatmap.

**Step 2: Configure packing**

- **Slot count**: Auto by default;
- calculated value `ceil(float_count / 64)`;
- optional manual slot count in Advanced settings;
- **Lambda (persistence weight)**, default `0.1`;
- **Mu (topology weight)**, default `0.01`;
- **Initial depth**, default `1`;
- **Gamma**, default `1.0`;
- estimated output size;
- padding warning for a partial final block.

Manual slot count below the required count is blocked because it would omit
input floats. A larger count is permitted but described as zero-padded slots.

**Step 3: Review and save**

- output `.crank` path;
- summary of input, selected tensor, float count, slot count, lambda, mu,
  depth, and gamma;
- **Pack weights** primary button.

### 6.2 Backend mapping

Existing calls:

1. `crankl_pack_n_slots(float_count)`
2. `crankl_pack_f32(data, count, out_slots, n_slots, lambda, mu)`
3. `crankl_compute_archive_metrics(out_slots, n_slots, &metrics)`
4. `crankl_cran_write(output, &header, slots, nullptr, nullptr)`

Safetensors requires the new public listing/reading API from section 2.2.

### 6.3 Result view

After success show:

- output path and **Open containing folder**;
- compression ratio: `(input_bytes / output_bytes)`;
- source versus reconstructed viewer;
- archive metrics;
- **Open archive**, **Optimize next**, and **Unpack test**.

## 7. Unpack and reconstruction

Export an archive as float32 values.

### 7.1 Controls

- input `.crank`;
- output `.f32`;
- mode:
  - **Reconstructed 8x8 blocks**, 64 floats per slot, default;
  - **Multivector coefficients**, 8 floats per slot, advanced/legacy;
- expected output float and byte count;
- **Unpack**.

### 7.2 Backend mapping

- `crankl_cran_read()`
- copy `cran.slots`
- `crankl_unpack_f32_mode()` with `CRANKL_UNPACK_DECRANK` or
  `CRANKL_UNPACK_COEFFS`
- `crankl_cran_close()`

The GUI writes floats through a checked, atomic file writer because the C API
does not provide a raw-float writer.

### 7.3 Visual result

Show the first reconstructed block, block navigation, coefficient table when
that mode is selected, output count, and output path.

## 8. Optimize: Turn

Apply discrete symplectic annealing to each crank slot. An optional target
guides updates toward lower reconstruction loss.

### 8.1 Layout and controls

The Optimize page has tabs **Turn** and **Finetune**.

Turn controls:

- input archive;
- optional target `.f32`;
- steps, default `1`, integer greater than zero;
- learning rate, default `0.01`, positive finite number;
- output archive;
- **Run turn**.

Advanced explanation:

- without a target: Verlet/BCH evolution and trit surgery favor Hamiltonian
  preservation;
- with a target: each slot uses reconstruction loss against the corresponding
  64-float block.

### 8.2 Backend mapping

For each step and slot:

- `crankl_turn(&slot, lr)` without a target; or
- `crankl_turn_toward(&slot, lr, target_block, target_len)` with a target.

After each step, copy all current slots into the history stack. Write through
`crankl_cran_write()`.

### 8.3 Visuals

Before running:

- current archive metrics;
- source/reconstruction view when target is present;
- expected history memory and output size.

While running:

- progress by step and slot;
- live current reconstruction loss when target is present;
- optional energy and loss line plots sampled at step boundaries.

After running:

- before/after reconstruction loss;
- before/after Clifford energy;
- changed slot count and normalized Hamming distance;
- source/current/error heatmaps;
- generated history layer count.

Accurate live progress and cancellation require a new stepped backend API or
running the existing per-slot APIs in the GUI worker. The first implementation
may use the per-slot APIs, but all archive writing remains in the adapter layer.

## 9. Optimize: Finetune

Minimize reconstruction loss and, optionally, holonomy task loss using
finite-difference trit updates and a first-order BCH group update. This is not
float backpropagation.

### 9.1 Controls

- input archive;
- reconstruction target `.f32`, optional but strongly recommended;
- **Enable task calibration** checkbox;
- calibration X `.f32`;
- calibration Y `.f32`;
- steps, default `200`;
- learning rate, default `0.02`;
- reconstruction weight, default `1.0`;
- task weight, default `0.1`;
- output archive;
- **Run finetune**.

When neither target nor calibration is supplied, block the run and direct the
user to Turn. When task calibration is enabled, both X and Y are required.

### 9.2 Backend mapping

- `crankl_decrank_frobenius_loss()` for initial and final reconstruction totals;
- `crankl_holonomy_mse()` for initial and final task loss;
- `crankl_finetune()` for one step at a time so progress can be emitted;
- `crankl_cran_write()` with copied layer snapshots.

The optional task callback passed to `crankl_finetune()` calls
`crankl_holonomy_mse()`.

### 9.3 Visuals

Show:

- reconstruction-loss plot by step;
- task-loss plot by step when calibration is enabled;
- weighted total-loss plot;
- before/after numeric summary;
- slot-change and Hamming summary;
- source/reconstructed/error heatmaps;
- calibration output versus expected output line plot.

The existing API returns no per-step diagnostic object. Because the GUI invokes
one step at a time, it can recompute loss at a configurable sample interval.
For large inputs the default is every 10 steps, with the final step always
sampled.

## 10. History and Peel

Restore an older full-slot snapshot from an archive's optimization history. If
validated history is absent, the low-level RG peel operation is available only
in the Advanced Math Lab and must not be presented as a checkpoint rollback.

### 10.1 Layout

The page has:

- archive selector and verification header;
- vertical history timeline from oldest to current;
- selected layer preview;
- layers-to-pop spin box;
- current versus selected-layer comparison;
- output path;
- **Restore selected layer**.

The preview shows changed slots, Hamming distance, metric deltas, and an 8x8
slot comparison.

### 10.2 Backend mapping

For validated history:

- `crankl_peel_stack(slots, n_slots, layers, stack_depth, layers_to_pop)`
- the future v3 full writer to retain remaining history and metadata.

For a single word in the Math Lab:

- `crankl_peel(&word, layers)`

## 11. Compare, Diff, and Resonance

Explain how two archives differ structurally, numerically, and geometrically.

### 11.1 Layout

Top row:

- Archive A drop target;
- swap button;
- Archive B drop target;
- **Compare**.

Summary strip:

- slots compared;
- slots changed;
- normalized bit Hamming distance;
- mean Clifford resonance;
- sheaf resonance (proxy);
- trit-density delta;
- Clifford-energy delta.

Main area:

- per-slot difference list;
- synchronized A/B/difference 8x8 heatmaps;
- metric comparison plot;
- per-slot Clifford resonance plot;
- filters: changed only, slot range, largest difference first.

Tabs:

- **Overview**
- **Slots**
- **Resonance**
- **Raw report**

### 11.2 Backend mapping

- `crankl_crank_diff_count()`
- `crankl_crank_diff_hamming()`
- `crankl_clifford_resonance()` per corresponding slot and averaged
- `crankl_sheaf_resonance()`
- `crankl_compute_archive_metrics()` for each archive
- `crankl_decrank_matrix()` for selected-slot visuals

The comparison uses `min(a_slots, b_slots)` and separately reports unmatched
slots. It must not silently hide a slot-count mismatch.

## 12. Inspect, Stats, and Verify

Provide one complete read-only archive audit. The separate CLI concepts
`inspect`, `stats`, and `verify` become sections of one page.

### 12.1 Layout

**Identity**

- path, bytes, version, flags, gamma;
- model name and source hash;
- copy buttons for paths/hashes.

**Verification**

- pass/fail;
- checksum and section-validation details when exposed;
- warnings and remediation text.

**Health metrics**

- shared metrics panel.

**Slots**

- shared slot browser.

**History and metadata**

- explicit presence and count;
- current format limitations.

**Export**

- **Export report JSON**
- **Copy summary**

### 12.2 Backend mapping

- `crankl_cran_read()`
- `crankl_cran_verify()`
- `crankl_cran_read_metadata()`
- `crankl_cran_compute_metrics()`
- `crankl_crank_to_multivector()`
- `crankl_decrank_matrix()`
- `crankl_cran_close()`

The existing `crankl_cran_verify()` result is only pass/fail. A future
`crankl_cran_validate_detailed()` should return an error code, byte offset, and
section name so the GUI can provide actionable diagnostics.

## 13. Bind archives

Combine corresponding crank slots with the Clifford product. This is a
geometric composition, not averaging or concatenation.

### 13.1 Controls and layout

- Archive A;
- Archive B;
- swap inputs;
- output path;
- compatibility summary;
- **Bind archives**.

Preview:

- A and B slot counts;
- output slot count `min(A, B)`;
- explicit warning for dropped unmatched slots;
- selected A/B/result multivectors;
- selected A/B/result 8x8 heatmaps;
- predicted output metrics from a sampled or full preview.

### 13.2 Backend mapping

For every corresponding pair:

- `crankl_bind(a_slot, b_slot)`

Then:

- `crankl_compute_archive_metrics()`
- `crankl_cran_write()`

The result currently carries neither input history nor reliable combined
metadata. The UI must say so before running.

## 14. Approximate forward transform: Holonomy

Apply the archive's path-ordered decranked operators to an input vector. This is
an approximate compressed-weight forward transform, not a general replacement
for a complete model runtime.

### 14.1 Input modes

- **Load vector file**: raw `.f32` or `.bin`;
- **Enter values**: editable table for small vectors;
- **Generate test vector**: zeros, ones, impulse, or seeded random values.

Controls:

- input archive;
- vector source;
- vector length;
- gamma display from archive;
- output `.f32` path;
- **Run forward transform**.

### 14.2 Backend mapping

- `crankl_holonomy(&cran, x, dim, y)`

For calibration comparison:

- `crankl_holonomy_mse(&cran, x, expected_y, dim)`

### 14.3 Visuals

- input and output line plots with index on the x-axis;
- delta plot `output - input`;
- table view for exact values;
- min, max, mean, norm;
- optional expected-output overlay and MSE.

Plots must downsample only for display; exported output contains every value.

## 15. Advanced Math Lab

The Math Lab is a separate developer workspace. It exposes low-level C APIs
without making them required knowledge for normal users.

### 15.1 Multivector editor

Fields:

- scalar;
- vector `e1`, `e2`, `e3`;
- bivector `e12`, `e23`, `e13`;
- pseudoscalar `e123`;
- depth;
- flags where supported.

Views:

- grade-grouped coefficient bars;
- crank word in hexadecimal and binary;
- bit-field labels;
- decranked 8x8 operator.

Actions and APIs:

- **Encode crank** -> `crankl_crank_from_multivector()`
- **Decode crank** -> `crankl_crank_to_multivector()`
- **Decrank matrix** -> `crankl_decrank_matrix()`

The UI previews trit quantization before encoding and warns that editing a
continuous coefficient may snap it to `-1`, `0`, or `+1`.

### 15.2 Trit inspector

Controls:

- select `-1`, `0`, or `+1`;
- show encoded two-bit value;
- enter a two-bit value and decode it.

APIs:

- `crankl_trit_encode()`
- `crankl_trit_decode()`

Reserved/invalid two-bit patterns show an error rather than being silently
accepted.

### 15.3 Clifford calculator

Two multivector inputs A and B with actions:

- **A times B** -> `crankl_clifford_product()`
- **Reverse A** -> `crankl_clifford_reversion()`
- **Resonance** -> encode each and call `crankl_clifford_resonance()`
- **Swap A/B**

Results show grade-grouped coefficients and the 8x8 operator. A note explains
that Clifford multiplication is generally order-dependent.

### 15.4 Sheaf explorer

Input:

- load one or two archives;
- choose a slot range;
- optionally enter a short list of crank words.

Visuals:

- slots as nodes in order;
- neighbor links when the current restriction proxy is non-zero;
- beta1 proxy;
- sheaf resonance (proxy) for two sequences.

APIs:

- `crankl_sheaf_beta1_proxy()`
- `crankl_sheaf_resonance()`

The current C API does not expose individual restriction-map values. To render
weighted edges accurately, add:

```c
double crankl_sheaf_restriction(uint64_t a, uint64_t b);
```

Without it, links are shown only as a conceptual sequence and the page must not
reimplement private formulas in the GUI.

### 15.5 Single-word Turn, Peel, and Bind

Provide a sandbox where all changes remain in memory until exported:

- `crankl_turn()`
- `crankl_turn_toward()`
- `crankl_peel()`
- `crankl_bind()`
- `crankl_decrank_frobenius_loss()`

Show before/after word, coefficients, matrix, energy proxy, Hamming change, and
target reconstruction loss.

### 15.6 Custom loss callbacks

The C API permits a `crankl_loss_fn`, but arbitrary native callbacks cannot be
made safely configurable through a normal form. The first GUI release exposes
the built-in reconstruction and holonomy losses only. A future plugin SDK may
allow signed native plugins; the GUI must not load arbitrary callback binaries
by default.

## 16. Jobs, progress, and cancellation

### 16.1 Job model

Each operation creates a `CranklJob` value with:

- UUID;
- operation type;
- source paths;
- destination paths;
- immutable parameter snapshot;
- state;
- progress numerator/denominator;
- start/end timestamps;
- result or structured error;
- temporary output paths.

Use `QThreadPool` and `QRunnable`, or worker `QObject`s moved to `QThread`.
Communicate only through queued signals and slots. Owned `std::vector` data may
cross worker boundaries by move; mmap-backed pointers may not.

### 16.2 Cancellation

Pack currently performs an internal annealing loop without a cancellation
callback. Cancellation requires backend job APIs:

```c
typedef int (*crankl_progress_fn)(size_t completed, size_t total, void *ctx);

int crankl_pack_f32_ex(..., crankl_progress_fn progress, void *ctx);
int crankl_finetune_ex(..., crankl_progress_fn progress, void *ctx);
```

The callback returns non-zero to cancel. Until these exist, cancellation is
cooperative only between slots or finetune steps.

### 16.3 Errors

Errors are shown with:

- operation and failing stage;
- input/output path;
- `crankl_strerror()` text when a Crankl status code exists;
- OS error text for file failures;
- a suggested correction;
- expandable technical details.

## 17. Implementation sequence

### Phase 1: Shell and read-only inspection

Builds:

- §3 Application Layout (main window, navigation, toolbar)
- §4 Home and workspace (file drop, empty/selected states)
- §5 Shared visual components (archive health header, metrics, slot browser,
  heatmaps)
- §2.3 Crank archives (open, verify, metrics, metadata copy/close)
- §12 Inspect, Stats, and Verify
- §16 Jobs, progress, and cancellation (job drawer and worker harness)

Tasks:

- Qt target, main window, navigation, settings, file drop;
- archive adapter with copied ownership and serialized reads;
- Inspect, Verify, Stats, slot browser, matrix heatmap;
- Jobs infrastructure and test harness.

### Phase 2: Import, Pack, Unpack, and Compare

Builds:

- §2.1 Raw float32 weights
- §2.2 Safetensors weights (including public list/read API)
- §2.4 Targets and calibration vectors (as shared import helpers)
- §2.5 Output files (`.crank`, `.f32`, report JSON)
- §5.4 Source versus reconstruction viewer
- §6 Pack weights
- §7 Unpack and reconstruction
- §11 Compare, Diff, and Resonance

Tasks:

- checked raw F32 reader/writer;
- public safetensors list/read API;
- Pack wizard and reconstruction viewer;
- Unpack;
- Compare, Diff, and Resonance;
- report export.

#### Phase 2 delivery (GUI)

The desktop GUI shipped the following live behavior on top of Phase 1's
read-only shell (all in `gui/`):

- ArchiveAdapter runs `open`/`compare` through the C API on a worker thread
  and reports `crankl_strerror()` text on failure.
- Pack / Turn / Finetune / Peel run as **real `crankl` CLI subprocesses**
  through `JobManager` (QProcess, FIFO queue, kill-on-cancel, bounded
  stdout/stderr capture, exit codes, parsed JSON summaries). The executable is
  resolved from `$CRANKL_CLI`, then QSettings `cli/path`, then a `crankl`
  sibling of the app binary, then `PATH`.
- New job dialog builds and previews the exact `crankl <command> …` invocation
  (§16 Jobs).
- Compare page (§11) shows per-slot changes with real hex words, hamming,
  resonance, and B−A metric deltas, with copy/export as JSON.
- Jobs drawer shows live stdout/stderr and exit codes per job (§16).
- Inspect result summary previews the `inspect --json` report inline.
- `--smoke-test` starts the full UI and exits 0 (works under
  `QT_QPA_PLATFORM=offscreen`).

### Phase 3: Mutation workflows

Builds:

- §8 Optimize: Turn
- §9 Optimize: Finetune
- §13 Bind archives
- §14 Approximate forward transform: Holonomy
- §16.2 Cancellation (cooperative cancel and later progress APIs)
- Pipeline page from left navigation (§3.2), composing §6 + §8 + §2.5
  manifests (no separate Pipeline section in this document)

Tasks:

- Turn and Finetune workers;
- loss plots and progress sampling;
- Bind;
- Holonomy;
- Pipeline and manifest generation;
- atomic outputs and cancellation improvements.

### Phase 4: Archive v3 and History

Builds:

- §2.3 Crank archives (format version, validated history count, rollback gate)
- §10 History and Peel
- Provenance-preserving outputs for §8, §9, and §13 once v3 writer exists

Tasks:

- per-handle mmap;
- explicit archive version/history fields;
- metadata plus history writer;
- safe History timeline and Peel;
- provenance-preserving Bind and optimization outputs.

History rollback must not ship before this phase.

### Phase 5: Advanced Math Lab and packaging

Builds:

- §15 Advanced Math Lab (multivector, trit, Clifford, sheaf, single-word tools)
- §5.5 Help page polish and packaging for the full §3 shell

Tasks:

- multivector, trit, Clifford, sheaf, and single-word tools;
- accessibility pass;
- performance profiling on large inputs;
- platform deployment and signing.
