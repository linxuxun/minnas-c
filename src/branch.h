#ifndef MINNAS_BRANCH_H
#define MINNAS_BRANCH_H
#include <time.h>
typedef struct BranchMgr BranchMgr;
typedef struct ReflogEntry { char *old_sha; char *new_sha; char *action; char *author; char *message; time_t time; } ReflogEntry;
BranchMgr *branchmgr_create(const char *repo_root, CAS *cas);
void branchmgr_free(BranchMgr *bm);
int branchmgr_create_branch(BranchMgr *bm, const char *name, const char *sha);
int branchmgr_delete_branch(BranchMgr *bm, const char *name);
// [name, sha, is_current (0/1), ...] flat array, caller frees outer
char **branchmgr_list_branches(BranchMgr *bm, int *count);
int branchmgr_checkout(BranchMgr *bm, const char *name);
char *branchmgr_get_current_sha(BranchMgr *bm);  // caller frees
char *branchmgr_get_current_branch(BranchMgr *bm);  // caller frees
int branchmgr_update_head(BranchMgr *bm, const char *sha, const char *action, const char *author, const char *msg);
ReflogEntry **branchmgr_get_reflog(BranchMgr *bm, int max_count, int *count);
void reflog_free(ReflogEntry **entries, int count);
#endif
