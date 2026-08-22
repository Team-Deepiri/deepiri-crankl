#ifndef CRANKL_INGEST_H
#define CRANKL_INGEST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRANKL_INGEST_NAME_MAX 128
#define CRANKL_INGEST_CHECKSUM_MAX 32

/* One tensor of a source checkpoint file (safetensors / GGUF). */
typedef struct crankl_source_tensor {
    char name[CRANKL_INGEST_NAME_MAX];
    uint64_t n_floats;
    uint32_t is_f32; /* 1 when the bytes are directly packable as f32 */
} crankl_source_tensor_t;

/* One entry of the multi-tensor index embedded in a packed archive footer. */
typedef struct crankl_archive_tensor {
    char name[CRANKL_INGEST_NAME_MAX];
    uint64_t slot_offset;
    uint64_t n_slots;
    uint64_t n_floats;
    char checksum[CRANKL_INGEST_CHECKSUM_MAX]; /* xxh64 hex of original f32 bytes */
} crankl_archive_tensor_t;

/* Enumerate tensors of a .safetensors file. Call count first, then list with
 * capacity >= count; CRANKL_ERR_INVALID signals the buffer was too small. */
int crankl_safetensors_count(const char *path, size_t *out_count);
int crankl_safetensors_list(const char *path, crankl_source_tensor_t *out, size_t capacity);

/* Same enumeration for GGUF checkpoints (all dtypes listed; only F32/F16 packable). */
int crankl_gguf_count(const char *path, size_t *out_count);
int crankl_gguf_list(const char *path, crankl_source_tensor_t *out, size_t capacity);

/* Pack EVERY f32 tensor into one multi-tensor archive (tensor index in the META
 * footer). manifest_path optionally receives a format_version 2 manifest. */
int crankl_pack_safetensors_multi(const char *path, const char *output_crank,
                                  const char *manifest_path, float alpha, float beta);

/* Single-tensor convenience: one archive per tensor (classic v2 layout). */
int crankl_pack_safetensors_tensor(const char *path, const char *tensor_name,
                                   const char *output_crank);

/* GGUF smoke ingest: read one tensor (F32 native or F16 widened) and pack it. */
int crankl_pack_gguf_f32(const char *path, const char *tensor_name, const char *output_crank);

/* Read back the multi-tensor index of an archive. count first, then list. */
int crankl_archive_tensor_count(const char *path, size_t *out_count);
int crankl_archive_tensor_list(const char *path, crankl_archive_tensor_t *out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
