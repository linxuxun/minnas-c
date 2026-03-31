#ifndef CAS_H
#define CAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declare Backend */
typedef struct Backend Backend;

typedef struct CAS {
    Backend *backend;
} CAS;

typedef struct {
    int object_count;
    size_t total_size;
} CAS_Stats;

CAS *cas_create(Backend *backend);
void cas_free(CAS *cas);

/* Store data, returns SHA-256 hex string (caller frees).
 * If same content already stored, returns existing hash (no new file). */
char *cas_store(CAS *cas, const uint8_t *data, size_t len);

/* Load data by SHA-256 hex. Returns 0 on success (caller frees *out_data) */
int cas_load(CAS *cas, const char *sha_hex, uint8_t **out_data, size_t *out_len);

/* Check if object exists */
bool cas_exists(CAS *cas, const char *sha_hex);

/* List all SHA hashes in store (caller frees each, and array) */
char **cas_list_all(CAS *cas, int *count);

/* Delete an object */
int cas_delete(CAS *cas, const char *sha_hex);

/* GC: remove unreachable objects (caller provides root SHAs) */
int cas_gc(CAS *cas, char **roots, int root_count, int *freed_count);

/* Stats */
void cas_stats(CAS *cas, CAS_Stats *stats);

/* Helper: compute SHA-256 hex of data (caller frees) */
char *cas_sha256_hex(const uint8_t *data, size_t len);

#endif /* CAS_H */
