#ifndef MINNAS_SNAPSHOT_H
#define MINNAS_SNAPSHOT_H

#include "cas.h"
#include <time.h>

// Snapshot metadata stored as JSON in CAS blob.
typedef struct {
    char sha[65];       // SHA-256 of the snapshot blob
    char *tree_json;     // JSON: {"path": "sha", ...}
    char *message;
    char *author;
    time_t timestamp;
    char *parent_sha;    // NULL if first snapshot (caller frees all)
} Snapshot;

// Create a new snapshot from tree JSON and store it.
// Returns newly allocated Snapshot (caller frees with snapshot_free).
Snapshot *snapshot_create(CAS *cas, const char *tree_json,
                          const char *message, const char *author,
                          const char *parent_sha);

// Retrieve a snapshot by SHA.
Snapshot *snapshot_get(CAS *cas, const char *sha_hex);

// Release snapshot memory.
void snapshot_free(Snapshot *s);

// Parse tree JSON into arrays.
// caller frees with tree_free().
typedef struct {
    char **paths;   // [n] paths
    char **shas;    // [n] corresponding blob SHAs
    int count;
} Tree;

Tree tree_parse(const char *tree_json);
void tree_free(Tree t);

// Diff two snapshots. Caller frees returned array with changes_free().
typedef struct {
    char action;   // 'A'dd, 'D'elete, 'M'odify
    char *path;
    char *old_sha;
    char *new_sha;
} Change;

Change **snapshot_diff(CAS *cas, const char *sha1, const char *sha2, int *out_count);
void changes_free(Change **changes, int count);

// Build tree JSON from flat arrays of paths and shas.
char *tree_build_json(char **paths, char **shas, int count);

#endif // MINNAS_SNAPSHOT_H
