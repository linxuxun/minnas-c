#ifndef MINNAS_H
#define MINNAS_H
#include "cas.h"
#include "snapshot.h"
#include "vfs.h"
#include "branch.h"
#include "namespace.h"

typedef struct Repo {
    char *root;
    Backend *backend;
    CAS *cas;
    NamespaceMgr *nsmgr;
    BranchMgr *branchmgr;
    VFS *vfs;
} Repo;

Repo *repo_init(const char *path, const char *backend_type);
Repo *repo_open(const char *path);
void repo_free(Repo *r);

char *repo_commit(Repo *r, const char *message, const char *author);
typedef struct { char **modified; char **added; char **deleted; int m, a, d; } RepoStatus;
RepoStatus *repo_status(Repo *r);
void repo_status_free(RepoStatus *s);
typedef struct { char *sha; char *message; char *author; time_t time; } RepoLogEntry;
RepoLogEntry **repo_log(Repo *r, int max_count, int *count);
void repo_log_free(RepoLogEntry **entries, int count);
Change **repo_diff(Repo *r, const char *sha1, const char *sha2, int *count);
Snapshot **repo_list_snapshots(Repo *r, int *count);
int repo_checkout_snapshot(Repo *r, const char *sha);
int repo_gc(Repo *r);
void repo_stats(Repo *r, CAS_Stats *cs, int *snap_count, int *branch_count);
#endif
