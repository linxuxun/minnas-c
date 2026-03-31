#include "branch.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>

struct BranchMgr {
    char *root;
    char *refs_dir;
    char *logs_dir;
    CAS *cas;
};

static char *join_path(const char *a, const char *b) {
    char *r = malloc(strlen(a) + strlen(b) + 3);
    sprintf(r, "%s/%s", a, b);
    return r;
}

static void ensure_dir(const char *path) {
    char tmp[1024];
    for (size_t i = 0; path[i]; i++) {
        if (path[i] == '/') {
            memcpy(tmp, path, i);
            tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) break;
        }
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {}
}

static void ensure_refs(BranchMgr *bm) {
    ensure_dir(bm->root);
    ensure_dir(bm->refs_dir);
    ensure_dir(bm->logs_dir);

    char *head_path = join_path(bm->root, "HEAD");
    FILE *f = fopen(head_path, "a");
    if (f) {
        long pos = ftell(f);
        if (pos == 0) fprintf(f, "ref: refs/heads/main\n");
        fclose(f);
    }
    free(head_path);

    char *main_path = join_path(bm->refs_dir, "main");
    FILE *mb = fopen(main_path, "a");
    if (mb) { long p = ftell(mb); if (p == 0) fprintf(mb, "\n"); fclose(mb); }
    free(main_path);

    char *log_dir = join_path(bm->logs_dir, "main");
    FILE *ld = fopen(log_dir, "a"); if (ld) fclose(ld);
    free(log_dir);
}

BranchMgr *branchmgr_create(const char *root, CAS *cas) {
    BranchMgr *bm = calloc(1, sizeof(BranchMgr));
    bm->root = strdup(root);
    bm->refs_dir = join_path(root, "refs/heads");
    bm->logs_dir = join_path(root, "logs/refs/heads");
    bm->cas = cas;
    ensure_refs(bm);
    return bm;
}

void branchmgr_free(BranchMgr *bm) {
    if (!bm) return;
    free(bm->root);
    free(bm->refs_dir);
    free(bm->logs_dir);
    free(bm);
}

static char *read_first_line(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[256] = {0};
    char *result = NULL;
    if (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && (*p == '\n' || *p == '\r')) p++;
        char *end = p + strlen(p) - 1;
        while (end > p && (*end == '\n' || *end == '\r')) *end-- = '\0';
        result = strdup(p);
    }
    fclose(f);
    return result ? result : strdup("");
}

static void write_ref(const char *path, const char *sha) {
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", sha ? sha : "");
        fclose(f);
    }
}

char *branchmgr_get_current_sha(BranchMgr *bm) {
    char *head = join_path(bm->root, "HEAD");
    char *ref = read_first_line(head);
    free(head);
    if (!ref || !*ref) { free(ref); return NULL; }
    if (strncmp(ref, "ref: ", 5) == 0) {
        char *ref_path = join_path(bm->root, ref + 5);
        char *sha = read_first_line(ref_path);
        free(ref_path);
        free(ref);
        return sha;
    }
    return ref;
}

char *branchmgr_get_current_branch(BranchMgr *bm) {
    char *head = join_path(bm->root, "HEAD");
    char *ref = read_first_line(head);
    free(head);
    if (!ref || !*ref) { free(ref); return NULL; }
    if (strncmp(ref, "ref: refs/heads/", 16) == 0) {
        char *name = strdup(ref + 16);
        free(ref);
        return name;
    }
    free(ref);
    return NULL;
}

int branchmgr_create_branch(BranchMgr *bm, const char *name, const char *sha) {
    char *path = join_path(bm->refs_dir, name);
    write_ref(path, sha ? sha : "");
    char *log_path = join_path(bm->logs_dir, name);
    FILE *lf = fopen(log_path, "a"); if (lf) fclose(lf);
    free(log_path);
    free(path);
    return 0;
}

int branchmgr_delete_branch(BranchMgr *bm, const char *name) {
    char *path = join_path(bm->refs_dir, name);
    int r = unlink(path);
    free(path);
    char *log_path = join_path(bm->logs_dir, name);
    unlink(log_path);
    free(log_path);
    return r;
}

int branchmgr_checkout(BranchMgr *bm, const char *name) {
    char *head = join_path(bm->root, "HEAD");
    FILE *f = fopen(head, "w");
    if (!f) { free(head); return -1; }
    fprintf(f, "ref: refs/heads/%s\n", name);
    fclose(f);
    free(head);
    return 0;
}

char **branchmgr_list_branches(BranchMgr *bm, int *count) {
    char *cur = branchmgr_get_current_branch(bm);
    char **result = NULL;
    int n = 0;
    DIR *d = opendir(bm->refs_dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (ent->d_name[0] == '.') continue;
            char *path = join_path(bm->refs_dir, ent->d_name);
            char *sha = read_first_line(path);
            int is_cur = (cur && strcmp(ent->d_name, cur) == 0);
            result = realloc(result, (size_t)(n + 3) * sizeof(char *));
            result[n++] = strdup(ent->d_name);
            result[n++] = sha ? sha : strdup("");
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", is_cur);
                result[n++] = strdup(buf);
            }
            free(path);
        }
        closedir(d);
    }
    free(cur);
    *count = n;
    return result;
}

int branchmgr_update_head(BranchMgr *bm, const char *sha,
                          const char *action, const char *author, const char *msg) {
    (void)action;
    char *head = join_path(bm->root, "HEAD");
    char *ref = read_first_line(head);
    free(head);
    if (!ref || !*ref) { free(ref); return -1; }

    char *ref_path = NULL;
    if (strncmp(ref, "ref: ", 5) == 0)
        ref_path = join_path(bm->root, ref + 5);

    char *old_sha = NULL;
    if (ref_path) {
        old_sha = read_first_line(ref_path);
        write_ref(ref_path, sha);

        // Append reflog
        char *log_rel = strstr(ref_path, "refs/heads/");
        if (log_rel) {
            char *log_path = join_path(bm->logs_dir, log_rel + 13);
            FILE *lf = fopen(log_path, "a");
            if (lf) {
                fprintf(lf, "%s %s commit %s %ld %s\n",
                        old_sha ? old_sha : "",
                        sha ? sha : "",
                        author ? author : "anonymous",
                        (long)time(NULL),
                        msg ? msg : "");
                fclose(lf);
            }
            free(log_path);
        }
        free(ref_path);
    }
    free(ref);
    free(old_sha);
    return 0;
}

ReflogEntry **branchmgr_get_reflog(BranchMgr *bm, int max_count, int *count) {
    *count = 0;
    char *branch = branchmgr_get_current_branch(bm);
    if (!branch) return NULL;
    char *log_path = join_path(bm->logs_dir, branch);
    free(branch);
    FILE *f = fopen(log_path, "r");
    free(log_path);
    ReflogEntry **entries = NULL;
    int n = 0;
    if (f) {
        char line[1024];
        while (n < max_count && fgets(line, sizeof(line), f)) {
            ReflogEntry *e = calloc(1, sizeof(ReflogEntry));
            char *p = line;
            char *fields[5] = {0};
            int fi = 0;
            while (*p && fi < 5) {
                while (*p && *p == ' ') p++;
                char *end = p;
                while (*end && *end != ' ' && *end != '\n') end++;
                char old = *end;
                *end = '\0';
                if (*p) fields[fi++] = strdup(p);
                if (old == '\0' || old == '\n') break;
                p = end + 1;
            }
            if (fields[0]) e->old_sha = fields[0];
            if (fields[1]) e->new_sha = fields[1];
            if (fields[2]) e->action = fields[2];
            if (fields[3]) e->author = fields[3];
            if (fields[4]) e->message = fields[4];
            e->time = fields[4] ? (time_t)strtod(fields[4], NULL) : 0;
            entries = realloc(entries, (size_t)(n + 1) * sizeof(void *));
            entries[n++] = e;
        }
        fclose(f);
    }
    *count = n;
    return entries;
}

void reflog_free(ReflogEntry **entries, int count) {
    for (int i = 0; i < count; i++) {
        free(entries[i]->old_sha);
        free(entries[i]->new_sha);
        free(entries[i]->action);
        free(entries[i]->author);
        free(entries[i]->message);
        free(entries[i]);
    }
    free(entries);
}
