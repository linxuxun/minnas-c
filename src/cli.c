#include "minnas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_RED    "\033[31m"
#define C_GRN    "\033[32m"
#define C_YEL    "\033[33m"
#define C_BLU    "\033[36m"
#define C_BLD    "\033[1m"
#define C_RST    "\033[0m"

static Repo *current_repo = NULL;
static char repo_path[256] = ".";

static void open_repo(const char *path) {
    if (current_repo) repo_free(current_repo);
    current_repo = repo_open(path);
    if (!current_repo) {
        fprintf(stderr, C_RED "Error: not a MinNAS repository: %s" C_RST "\n", path);
    }
}

static void cmd_init(int argc, char **argv) {
    const char *path = "."; const char *backend = "local";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--path") == 0)
            path = argv[++i];
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--backend") == 0)
            backend = argv[++i];
    }
    Repo *r = repo_init(path, backend);
    if (r) {
        printf(C_GRN "✓" C_RST " Initialized MinNAS repository at %s\n", path);
        printf("  backend: " C_BLU "%s" C_RST "\n", backend);
        repo_free(r);
    } else {
        fprintf(stderr, C_RED "✗ Init failed" C_RST "\n");
    }
}

static void cmd_status(void) {
    open_repo(repo_path);
    if (!current_repo) return;
    printf(C_BLD "MinNAS Status" C_RST "\n");
    char *branch = branchmgr_get_current_branch(current_repo->branchmgr);
    char *sha = branchmgr_get_current_sha(current_repo->branchmgr);
    printf("  branch:   " C_BLU "%s" C_RST "\n", branch ? branch : "(detached)");
    printf("  commit:   " C_BLU "%.8s" C_RST "\n", sha ? sha : "none");
    free(branch); free(sha);
    printf("  " C_GRN "(clean)" C_RST "\n");
}

static void cmd_commit(int argc, char **argv) {
    if (argc == 0) { fprintf(stderr, C_RED "Usage: commit <message>" C_RST "\n"); return; }
    open_repo(repo_path);
    if (!current_repo) return;
    const char *msg = argv[0];
    const char *author = "anonymous";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--author") == 0)
            author = argv[++i];
    }
    char *sha = repo_commit(current_repo, msg, author);
    if (sha) {
        printf(C_GRN "✓" C_RST " Committed: " C_BLU "%.8s" C_RST " %s\n", sha, msg);
        free(sha);
    } else {
        fprintf(stderr, C_RED "✗ Commit failed" C_RST "\n");
    }
}

static void cmd_log(int argc, char **argv) {
    int n = 10;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) n = atoi(argv[++i]);
    }
    open_repo(repo_path);
    if (!current_repo) return;
    int count;
    RepoLogEntry **entries = repo_log(current_repo, n, &count);
    for (int i = 0; i < count; i++) {
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", localtime(&entries[i]->time));
        printf(C_BLU "%.8s" C_RST " %s " C_YEL "%s" C_RST "\n    %s\n",
               entries[i]->sha, ts, entries[i]->author, entries[i]->message);
    }
    repo_log_free(entries, count);
}

static void cmd_snapshot_list(void) {
    open_repo(repo_path);
    if (!current_repo) return;
    int count;
    Snapshot **snaps = repo_list_snapshots(current_repo, &count);
    if (!snaps || count == 0) {
        printf("(no snapshots)\n");
    } else {
        for (int i = 0; i < count; i++) {
            printf(C_BLU "%.8s" C_RST " %s\n", snaps[i]->sha, snaps[i]->message);
            snapshot_free(snaps[i]);
        }
        free(snaps);
    }
}

static void cmd_snapshot_show(const char *sha) {
    open_repo(repo_path);
    if (!current_repo) return;
    Snapshot *s = snapshot_get(current_repo->cas, sha);
    if (!s) { fprintf(stderr, C_RED "✗ Snapshot not found" C_RST "\n"); return; }
    printf("SHA:     %.64s\n", s->sha);
    printf("Message:  %s\n", s->message);
    printf("Author:   %s\n", s->author);
    printf("Parent:   %s\n", s->parent_sha ? s->parent_sha : "(none)");
    printf("Files:    %d\n", s->tree_json ? 1 : 0);
    Tree t = tree_parse(s->tree_json);
    for (int i = 0; i < t.count; i++)
        printf("  %.8s  %s\n", t.shas[i], t.paths[i]);
    tree_free(t);
    snapshot_free(s);
}

static void cmd_branch_list(void) {
    open_repo(repo_path);
    if (!current_repo) return;
    int count;
    char **branches = branchmgr_list_branches(current_repo->branchmgr, &count);
    for (int i = 0; i < count; i += 3) {
        const char *name = branches[i];
        const char *sha = branches[i+1];
        int is_cur = atoi(branches[i+2]);
        printf("  %s " C_BLU "%-20s" C_RST " %.8s%s\n",
               is_cur ? "→" : " ", name, sha, is_cur ? " (current)" : "");
        free(branches[i]); free(branches[i+1]); free(branches[i+2]);
    }
    free(branches);
}

static void cmd_gc(void) {
    open_repo(repo_path);
    if (!current_repo) return;
    int freed = repo_gc(current_repo);
    printf(C_GRN "✓" C_RST " GC freed %d object(s)\n", freed);
}

static void cmd_stats(void) {
    open_repo(repo_path);
    if (!current_repo) return;
    CAS_Stats cs; int sc, bc;
    repo_stats(current_repo, &cs, &sc, &bc);
    printf(C_BLD "Repository Stats" C_RST "\n");
    printf("  Objects:     %d\n", cs.object_count);
    printf("  Total size: %zu bytes\n", cs.total_size);
    printf("  Branches:    %d\n", bc);
    printf("  Snapshots:   %d\n", sc);
}

static void cmd_fs_ls(const char *path) {
    open_repo(repo_path);
    if (!current_repo) return;
    int count;
    char **entries = vfs_listdir(current_repo->vfs, path ? path : "/", &count);
    for (int i = 0; i < count; i++) {
        printf("  %s\n", entries[i]);
        free(entries[i]);
    }
    free(entries);
}

static void cmd_fs_cat(const char *path) {
    open_repo(repo_path);
    if (!current_repo) return;
    int fd = vfs_open(current_repo->vfs, path, "r");
    if (fd < 0) { fprintf(stderr, C_RED "✗ Cannot open: %s" C_RST "\n", path); return; }
    uint8_t buf[4096]; int n;
    while ((n = vfs_read(current_repo->vfs, fd, buf, sizeof(buf))) > 0)
        fwrite(buf, 1, (size_t)n, stdout);
    vfs_close(current_repo->vfs, fd);
}

static void cmd_fs_write(const char *path, const char *content) {
    open_repo(repo_path);
    if (!current_repo) return;
    int fd = vfs_open(current_repo->vfs, path, "w");
    if (fd < 0) { fprintf(stderr, C_RED "✗ Cannot open: %s" C_RST "\n", path); return; }
    vfs_write(current_repo->vfs, fd, (uint8_t *)content, (int)strlen(content));
    vfs_close(current_repo->vfs, fd);
    printf(C_GRN "✓" C_RST " Wrote %zu bytes to %s\n", strlen(content), path);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf(C_BLD "MiniNAS" C_RST " - Git-style file storage with snapshots\n");
        printf("Usage: minnas <command> [args]\n");
        printf("Commands: init, status, commit, log, snapshot, branch, fs, gc, stats\n");
        return 0;
    }

    if (strcmp(argv[1], "init") == 0) cmd_init(argc - 2, argv + 2);
    else if (strcmp(argv[1], "status") == 0) cmd_status();
    else if (strcmp(argv[1], "commit") == 0) cmd_commit(argc - 2, argv + 2);
    else if (strcmp(argv[1], "log") == 0) cmd_log(argc - 2, argv + 2);
    else if (strcmp(argv[1], "snapshot") == 0) {
        if (argc >= 3 && strcmp(argv[2], "list") == 0) cmd_snapshot_list();
        else if (argc >= 4 && strcmp(argv[2], "show") == 0) cmd_snapshot_show(argv[3]);
        else if (argc >= 4 && strcmp(argv[2], "checkout") == 0) {
            open_repo(repo_path);
            if (current_repo) repo_checkout_snapshot(current_repo, argv[3]);
        }
        else cmd_snapshot_list();
    }
    else if (strcmp(argv[1], "branch") == 0) {
        if (argc >= 3 && strcmp(argv[2], "-a") == 0) cmd_branch_list();
        else cmd_branch_list();
    }
    else if (strcmp(argv[1], "gc") == 0) cmd_gc();
    else if (strcmp(argv[1], "stats") == 0) cmd_stats();
    else if (strcmp(argv[1], "fs") == 0) {
        if (argc >= 3 && strcmp(argv[2], "ls") == 0) cmd_fs_ls(argc >= 4 ? argv[3] : "/");
        else if (argc >= 4 && strcmp(argv[2], "cat") == 0) cmd_fs_cat(argv[3]);
        else if (argc >= 5 && strcmp(argv[2], "write") == 0) cmd_fs_write(argv[3], argv[4]);
        else cmd_fs_ls("/");
    }
    else {
        fprintf(stderr, C_RED "Unknown command: %s" C_RST "\n", argv[1]);
        return 1;
    }

    if (current_repo) repo_free(current_repo);
    return 0;
}
