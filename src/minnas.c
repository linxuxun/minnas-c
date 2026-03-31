#include "minnas.h"
#include "blob.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

static char *join(const char *a, const char *b) {
    char *r = malloc(strlen(a) + strlen(b) + 3);
    sprintf(r, "%s/%s", a, b); return r;
}

static void ensure_dir(const char *path) {
    char tmp[1024]; struct stat st;
    for (size_t i = 0; path[i]; i++) {
        if (path[i] == '/') {
            memcpy(tmp, path, i); tmp[i] = '\0';
            if (stat(tmp, &st) != 0) mkdir(tmp, 0755);
        }
    }
    if (stat(path, &st) != 0) mkdir(path, 0755);
}

Repo *repo_init(const char *path, const char *backend_type) {
    Repo *r = calloc(1, sizeof(Repo));
    r->root = strdup(path);
    ensure_dir(r->root);

    Backend *be = backend_local_create(r->root);
    r->backend = be;
    r->cas = cas_create(be);

    r->nsmgr = nsmgr_create(r->root, r->cas);
    r->branchmgr = branchmgr_create(r->root, r->cas);

    Tree init_tree = {0};
    r->vfs = vfs_create(r->cas, &init_tree);
    return r;
}

Repo *repo_open(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    return repo_init(path, "local");
}

void repo_free(Repo *r) {
    if (!r) return;
    vfs_free(r->vfs);
    branchmgr_free(r->branchmgr);
    nsmgr_free(r->nsmgr);
    cas_free(r->cas);
    backend_free(r->backend);
    free(r->root);
    free(r);
}

char *repo_commit(Repo *r, const char *message, const char *author) {
    char *tree_json = vfs_commit(r->vfs);
    char *current_sha = branchmgr_get_current_sha(r->branchmgr);
    Snapshot *snap = snapshot_create(r->cas, tree_json, message, author, current_sha);
    free(tree_json);
    if (!snap) return NULL;
    branchmgr_update_head(r->branchmgr, snap->sha, "commit", author, message);
    nsmgr_set_current_tree_sha(r->nsmgr, snap->sha);
    char *sha = strdup(snap->sha);
    snapshot_free(snap);
    free(current_sha);
    return sha;
}

typedef struct { char **modified; char **added; char **deleted; int m, a, d; } RepoStatus;
RepoStatus *repo_status(Repo *r) {
    RepoStatus *s = calloc(1, sizeof(RepoStatus));
    (void)r; (void)s;
    // Compare vfs tree vs current snapshot
    return s;
}
void repo_status_free(RepoStatus *s) {
    if (!s) return;
    for (int i = 0; i < s->m; i++) free(s->modified[i]);
    for (int i = 0; i < s->a; i++) free(s->added[i]);
    for (int i = 0; i < s->d; i++) free(s->deleted[i]);
    free(s->modified); free(s->added); free(s->deleted);
    free(s);
}

typedef struct { char *sha; char *message; char *author; time_t time; } RepoLogEntry;
RepoLogEntry **repo_log(Repo *r, int max_count, int *count) {
    ReflogEntry **rl = branchmgr_get_reflog(r->branchmgr, max_count > 0 ? max_count : 50, count);
    RepoLogEntry **entries = malloc((size_t)*count * sizeof(RepoLogEntry *));
    for (int i = 0; i < *count; i++) {
        entries[i] = malloc(sizeof(RepoLogEntry));
        entries[i]->sha = strdup(rl[i]->new_sha);
        entries[i]->message = strdup(rl[i]->message);
        entries[i]->author = strdup(rl[i]->author);
        entries[i]->time = rl[i]->time;
    }
    reflog_free(rl, *count);
    return entries;
}
void repo_log_free(RepoLogEntry **entries, int count) {
    for (int i = 0; i < count; i++) {
        free(entries[i]->sha); free(entries[i]->message); free(entries[i]->author);
        free(entries[i]);
    }
    free(entries);
}

Change **repo_diff(Repo *r, const char *sha1, const char *sha2, int *count) {
    return snapshot_diff(r->cas, sha1, sha2, count);
}

Snapshot **repo_list_snapshots(Repo *r, int *count) {
    (void)r; *count = 0; return NULL;
}

int repo_checkout_snapshot(Repo *r, const char *sha) {
    return vfs_checkout(r->vfs, sha);
}

int repo_gc(Repo *r) {
    char **roots = NULL; int root_count = 0;
    char *sha = branchmgr_get_current_sha(r->branchmgr);
    if (sha) {
        roots = &sha; root_count = 1;
    }
    int freed = 0;
    cas_gc(r->cas, roots, root_count, &freed);
    if (sha) free(sha);
    return freed;
}

void repo_stats(Repo *r, CAS_Stats *cs, int *snap_count, int *branch_count) {
    if (cs) cas_stats(r->cas, cs);
    if (branch_count) {
        int bc; branchmgr_list_branches(r->branchmgr, &bc);
        *branch_count = bc / 3;
    }
    if (snap_count) *snap_count = 0;
}
