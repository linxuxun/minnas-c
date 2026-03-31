#include "namespace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

struct NamespaceMgr {
    char *root;
    CAS *cas;
    char *current_ns;
};

static void ensure_dir(const char *p) {
    char t[1024];
    for (size_t i = 0; p[i]; i++) {
        if (p[i] == '/') {
            memcpy(t, p, i);
            t[i] = '\0';
            if (mkdir(t, 0755) != 0 && errno != EEXIST) break;
        }
    }
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {}
}

static char *join(const char *a, const char *b) {
    char *r = malloc(strlen(a) + strlen(b) + 3);
    sprintf(r, "%s/%s", a, b);
    return r;
}

static void init_ns_dir(const char *ns_root, const char *name) {
    char *nr = join(ns_root, name);
    ensure_dir(nr);
    char *snap = join(nr, "snapshots");
    ensure_dir(snap);
    free(snap);
    free(nr);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return strdup("default");
    char line[256] = {0};
    if (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && (*p == '\n' || *p == '\r' || *p == ' ')) p++;
        char *e = p + strlen(p) - 1;
        while (e > p && (*e == '\n' || *e == '\r')) *e-- = '\0';
        fclose(f);
        return *p ? strdup(p) : strdup("default");
    }
    fclose(f);
    return strdup("default");
}

static void write_file(const char *path, const char *val) {
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", val ? val : "");
        fclose(f);
    }
}

NamespaceMgr *nsmgr_create(const char *root, CAS *cas) {
    NamespaceMgr *nm = calloc(1, sizeof(NamespaceMgr));
    nm->root = strdup(root);
    nm->cas = cas;
    ensure_dir(nm->root);
    ensure_dir(join(nm->root, "namespaces"));
    init_ns_dir(join(nm->root, "namespaces"), "default");
    char *ns_file = join(nm->root, "_current_ns");
    nm->current_ns = read_file(ns_file);
    write_file(ns_file, nm->current_ns);
    free(ns_file);
    return nm;
}

void nsmgr_free(NamespaceMgr *nm) {
    if (!nm) return;
    free(nm->root);
    free(nm->current_ns);
    free(nm);
}

int nsmgr_create_namespace(NamespaceMgr *nm, const char *name) {
    init_ns_dir(join(nm->root, "namespaces"), name);
    return 0;
}

int nsmgr_delete_namespace(NamespaceMgr *nm, const char *name) {
    if (strcmp(name, "default") == 0) { errno = EINVAL; return -1; }
    char *nr = join(join(nm->root, "namespaces"), name);
    rmdir(nr);
    free(nr);
    return 0;
}

char **nsmgr_list_namespaces(NamespaceMgr *nm, int *count) {
    char *nd = join(nm->root, "namespaces");
    char **result = NULL;
    int n = 0;
    result = realloc(result, (size_t)(n + 1) * sizeof(char *));
    result[n++] = strdup("default");
    DIR *d = opendir(nd);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (ent->d_name[0] == '.') continue;
            result = realloc(result, (size_t)(n + 1) * sizeof(char *));
            result[n++] = strdup(ent->d_name);
        }
        closedir(d);
    }
    free(nd);
    *count = n;
    return result;
}

int nsmgr_switch_namespace(NamespaceMgr *nm, const char *name) {
    char *nd = join(nm->root, "namespaces");
    struct stat st;
    int ok = (stat(nd, &st) == 0 && S_ISDIR(st.st_mode));
    char *nr = join(nd, name);
    if (stat(nr, &st) != 0 || !S_ISDIR(st.st_mode)) ok = 0;
    free(nd);
    free(nr);
    if (!ok) { errno = ENOENT; return -1; }
    free(nm->current_ns);
    nm->current_ns = strdup(name);
    char *ns_file = join(nm->root, "_current_ns");
    write_file(ns_file, nm->current_ns);
    free(ns_file);
    return 0;
}

char *nsmgr_get_current(NamespaceMgr *nm) {
    return strdup(nm->current_ns ? nm->current_ns : "default");
}

char *nsmgr_get_current_tree_sha(NamespaceMgr *nm) {
    char *path = join(join(nm->root, "namespaces"),
                      nm->current_ns ? nm->current_ns : "default");
    char *ct = join(path, "current_tree");
    char *sha = read_file(ct);
    free(ct);
    free(path);
    return *sha ? sha : (free(sha), NULL);
}

int nsmgr_set_current_tree_sha(NamespaceMgr *nm, const char *sha) {
    char *path = join(join(nm->root, "namespaces"),
                      nm->current_ns ? nm->current_ns : "default");
    char *ct = join(path, "current_tree");
    write_file(ct, sha);
    free(ct);
    free(path);
    return 0;
}
