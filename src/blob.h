#ifndef MINNAS_BLOB_H
#define MINNAS_BLOB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Blob format: "blob {size}\0{data...}"
// Stored in CAS as-is (no extra header).

// Build a blob from raw data.
// Returns newly allocated blob bytes (caller frees), or NULL on error.
// Sets *out_len to blob size.
uint8_t *blob_build(const uint8_t *data, size_t data_len, size_t *out_len);

// Read the data portion from a blob.
// Returns NULL if blob is malformed.
// Sets *out_data and *out_len.
uint8_t *blob_read(const uint8_t *blob, size_t blob_len,
                    size_t *out_data_len);

// Verify blob integrity: check header + size field match blob_len.
bool blob_verify(const uint8_t *blob, size_t blob_len);

// Get human-readable description of a blob (for debugging).
// Returns static buffer, NOT caller-owned.
const char *blob_describe(const uint8_t *blob, size_t blob_len);

#endif // MINNAS_BLOB_H
