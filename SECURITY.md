# Security

## Input limits

Crankl rejects oversized or malformed inputs before mmap, allocation, or parsing:

| Surface | Limit |
|---------|-------|
| `.crank` file size | 1 GiB |
| `.f32` payload | 256 MiB |
| crank slots per archive | 1M |
| finetune layer stack depth | 64k |
| safetensors JSON header | 16 MiB |
| safetensors tensor slice | 512 MiB |

## `.crank` validation

- Magic, version, and xxHash checksum are verified on read.
- Slot count and v2 layer-stack tail length are bounds-checked against payload size.
- Integer overflow is guarded when computing `n_slots * 8` and stack byte lengths.

## CLI I/O

- `read_f32` checks `fread` return value and rejects truncated files.
- Safetensors offsets must stay within the file and tensor byte length must be a multiple of 4.

## Reporting

Report vulnerabilities to the Deepiri security contact for your organization. Do not open public issues for undisclosed security bugs.
