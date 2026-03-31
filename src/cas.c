#include "cas.h"
#include "backend.h"
#include "sha256.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

CAS *cas_create(Backend *backend) {
    CAS *cas = calloc(1, sizeof(CAS));
    if (!cas) return NULL;
    cas->backend = backend;
    return cas;
}

void cas_free(CAS *cas) {
    if (!cas) return;
    free(cas);
}

char *cas_sha256_hex(const uint8_t *data, size_t len) {
    return sha256_hex(data, len);
}

char *cas_store(CAS *cas, const uint8_t *data, size_t len) {
    if (!cas || !data) return NULL;
    char *sha_hex = sha256_hex(data, len);
    if (!sha_hex) return NULL;

    /* Deduplication: check if already exists */
    if (cas->backend->ops->exists(cas->backend->ctx, sha_hex)) {
        return sha_hex;  /* already stored */
    }

    /* Store via backend */
    if (cas->backend->ops->write(cas->backend->ctx, sha_hex, data, len) != 0) {
        free(sha_hex);
        return NULL;
    }
    return sha_hex;
}

int cas_load(CAS *cas, const char *sha_hex, uint8_t **out_data, size_t *out_len) {
    if (!cas || !sha_hex || !out_data || !out_len) return -1;
    *out_data = NULL;
    *out_len = 0;

    if (strlen(sha_hex) != 64) { errno = EINVAL; return -1; }
    return cas->backend->ops->read(cas->backend->ctx, sha_hex, out_data, out_len);
}

bool cas_exists(CAS *cas, const char *sha_hex) {
    if (!cas || !sha_hex) return false;
    return cas->backend->ops->exists(cas->backend->ctx, sha_hex);
}

char **cas_list_all(CAS *cas, int *count) {
    if (!cas || !count) return NULL;
    return cas->backend->ops->list_all(cas->backend->ctx, count);
}

int cas_delete(CAS *cas, const char *sha_hex) {
    if (!cas || !sha_hex) return -1;
    return cas->backend->ops->delete(cas->backend->ctx, sha_hex);
}

int cas_gc(CAS *cas, char **roots, int root_count, int *freed_count) {
    if (!cas || !freed_count) return -1;
    *freed_count = 0;

    int total = 0;
    char **all = cas_list_all(cas, &total);
    if (!all) return 0;

    /* Build reachable set using BFS */
    HashTable *reachable = ht_create();
    if (!reachable) { free(all); return -1; }

    for (int i = 0; i < root_count; i++) {
        if (roots[i]) ht_set(reachable, roots[i], (void*)1);
    }

    /* Simple GC: we rely on backend to track reference counts.
     * For now, just report what's in the store. */
    (void)reachable;
    /* TODO: Implement proper reachability analysis */
    (void)all;
    ht_free(reachable);
    *freed_count = 0;
    return 0;
}

void cas_stats(CAS *cas, CAS_Stats *stats) {
    if (!cas || !stats) return;
    stats->object_count = 0;
    stats->total_size = 0;

    int count = 0;
    char **all = cas_list_all(cas, &count);
    if (!all) return;

    stats->object_count = count;
    for (int i = 0; i < count; i++) {
        uint8_t *data = NULL;
        size_t len = 0;
        if (cas->backend->ops->read(cas->backend->ctx, all[i], &data, &len) == 0) {
            stats->total_size += len;
            free(data);
        }
        free(all[i]);
    }
    free(all);
}
