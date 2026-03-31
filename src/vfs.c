#include "vfs.h"
#include "blob.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define MAX_OPEN_FILES 128

typedef struct VFile {
    char *path;
    char mode;
    bool modified;
    char *blob_sha;
    uint8_t *buffer;
    size_t buf_size;
    size_t buf_cap;
    size_t position;
    bool closed;
} VFile;

struct VFS {
    CAS *cas;
    Tree tree;
    VFile *files[MAX_OPEN_FILES];
    int next_fd;
    char error_msg[256];
};

const char *vfs_error(VFS *v) { return v->error_msg; }

static char *norm_path(const char *p) {
    while (*p == '/') p++;
    return (char *)p;
}

static int tree_idx(VFS *v, const char *path) {
    char *p = norm_path(path);
    for (int i = 0; i < v->tree.count; i++)
        if (strcmp(v->tree.paths[i], p) == 0) return i;
    return -1;
}

static void update_tree(VFS *v, const char *path, const char *sha) {
    char *p = norm_path(path);
    int i = tree_idx(v, p);
    if (i >= 0) { free(v->tree.shas[i]); v->tree.shas[i] = strdup(sha); }
    else {
        v->tree.paths = realloc(v->tree.paths, (size_t)(++v->tree.count) * sizeof(char *));
        v->tree.shas  = realloc(v->tree.shas,  (size_t)v->tree.count * sizeof(char *));
        v->tree.paths[v->tree.count-1] = strdup(p);
        v->tree.shas[v->tree.count-1]  = strdup(sha);
    }
}

VFS *vfs_create(CAS *cas, Tree *init) {
    VFS *v = calloc(1, sizeof(VFS));
    v->cas = cas;
    if (init) {
        v->tree.count = init->count;
        v->tree.paths = malloc(sizeof(char *) * (size_t)init->count);
        v->tree.shas  = malloc(sizeof(char *) * (size_t)init->count);
        for (int i = 0; i < init->count; i++) {
            v->tree.paths[i] = strdup(init->paths[i]);
            v->tree.shas[i]  = strdup(init->shas[i]);
        }
    }
    return v;
}

void vfs_free(VFS *v) {
    if (!v) return;
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        if (v->files[i] && !v->files[i]->closed) vfs_close(v, i + 3), free(v->files[i]);
    tree_free(v->tree);
    free(v);
}

static int alloc_fd(VFS *v, VFile *f) {
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        if (!v->files[i]) { v->files[i] = f; return i + 3; }
    free(f); errno = EMFILE; return -1;
}

static VFile *get_vf(VFS *v, int fd) {
    if (fd < 3 || fd >= 3+MAX_OPEN_FILES) { errno = EBADF; return NULL; }
    VFile *f = v->files[fd - 3];
    if (!f) { errno = EBADF; return NULL; }
    return f;
}

int vfs_open(VFS *v, const char *path, const char *mode) {
    VFile *f = calloc(1, sizeof(VFile));
    f->path = strdup(norm_path(path));
    f->mode = mode[0];
    f->modified = false;
    f->buffer = malloc(4096);
    f->buf_cap = 4096;
    f->buf_size = 0;
    f->position = 0;
    f->closed = false;
    f->blob_sha = NULL;

    int idx = tree_idx(v, path);
    if (mode[0] == 'w') {
        if (idx >= 0) f->blob_sha = strdup(v->tree.shas[idx]);
    } else if (mode[0] == 'a') {
        f->mode = 'a';
        if (idx >= 0) {
            f->blob_sha = strdup(v->tree.shas[idx]);
            uint8_t *d = NULL; size_t dl = 0;
            if (cas_read(v->cas, v->tree.shas[idx], &d, &dl)) {
                size_t x; uint8_t *inner = blob_read(d, dl, &x);
                if (inner) {
                    if (x > f->buf_cap) f->buffer = realloc(f->buffer, f->buf_cap = x*2);
                    memcpy(f->buffer, inner, x); f->buf_size = x; free(inner);
                }
                free(d);
            }
        }
    } else {
        f->mode = 'r';
        if (idx < 0) { free(f); errno = ENOENT; return -1; }
        f->blob_sha = strdup(v->tree.shas[idx]);
        uint8_t *d = NULL; size_t dl = 0;
        if (cas_read(v->cas, v->tree.shas[idx], &d, &dl)) {
            size_t x; uint8_t *inner = blob_read(d, dl, &x);
            if (inner) {
                if (x > f->buf_cap) f->buffer = realloc(f->buffer, f->buf_cap = x*2);
                memcpy(f->buffer, inner, x); f->buf_size = x; free(inner);
            }
            free(d);
        }
    }
    return alloc_fd(v, f);
}

int vfs_close(VFS *v, int fd) {
    VFile *f = get_vf(v, fd);
    if (!f || f->closed) { errno = EBADF; return -1; }
    f->closed = true;
    if (f->modified || f->buf_size > 0) {
        size_t bl; uint8_t *blob = blob_build(f->buffer, f->buf_size, &bl);
        if (blob) {
            uint8_t dig[32]; sha256_hash(blob, bl, dig);
            char hx[65]; sha256_hex_to(hx, dig);
            cas_write(v->cas, hx, blob, bl);
            update_tree(v, f->path, hx);
            free(blob);
        }
    }
    v->files[fd - 3] = NULL;
    free(f->path); free(f->blob_sha); free(f->buffer); free(f);
    return 0;
}

int vfs_read(VFS *v, int fd, uint8_t *buf, int n) {
    VFile *f = get_vf(v, fd);
    if (!f) return -1;
    size_t rem = f->buf_size > f->position ? f->buf_size - f->position : 0;
    size_t r = (size_t)n < rem ? (size_t)n : rem;
    memcpy(buf, f->buffer + f->position, r); f->position += r;
    return (int)r;
}

int vfs_write(VFS *v, int fd, const uint8_t *buf, int n) {
    VFile *f = get_vf(v, fd);
    if (!f) return -1;
    if (f->mode == 'r') { errno = EBADF; return -1; }
    f->modified = true;
    if (f->mode == 'a') f->position = f->buf_size;
    size_t need = f->position + (size_t)n;
    if (need > f->buf_cap) {
        size_t nc = need * 2;
        uint8_t *nb = realloc(f->buffer, nc);
        if (!nb) return -1;
        f->buffer = nb; f->buf_cap = nc;
    }
    memcpy(f->buffer + f->position, buf, (size_t)n);
    f->position += (size_t)n;
    if (f->position > f->buf_size) f->buf_size = f->position;
    return n;
}

int64_t vfs_lseek(VFS *v, int fd, int64_t off, int whence) {
    VFile *f = get_vf(v, fd);
    if (!f) return -1;
    size_t np = 0;
    if (whence == 0) np = (size_t)off;
    else if (whence == 1) np = f->position + (size_t)off;
    else if (whence == 2) np = f->buf_size + (size_t)off;
    else { errno = EINVAL; return -1; }
    f->position = np;
    return (int64_t)np;
}

int64_t vfs_tell(VFS *v, int fd) {
    VFile *f = get_vf(v, fd);
    return f ? (int64_t)f->position : -1;
}

int vfs_truncate(VFS *v, const char *path, off_t sz) {
    int fd = vfs_open(v, path, "w");
    if (fd < 0) return -1;
    VFile *f = get_vf(v, fd);
    if (f && (size_t)sz < f->buf_size) {
        f->buf_size = (size_t)sz;
        if (f->position > f->buf_size) f->position = f->buf_size;
        f->modified = true;
    }
    vfs_close(v, fd);
    return 0;
}

int vfs_rm(VFS *v, const char *path) {
    char *p = norm_path(path);
    for (int i = 0; i < v->tree.count; i++) {
        if (strcmp(v->tree.paths[i], p) == 0) {
            free(v->tree.paths[i]); free(v->tree.shas[i]);
            for (int j = i; j < v->tree.count - 1; j++) {
                v->tree.paths[j] = v->tree.paths[j+1];
                v->tree.shas[j] = v->tree.shas[j+1];
            }
            v->tree.count--;
            return 0;
        }
    }
    errno = ENOENT; return -1;
}

bool vfs_exists(VFS *v, const char *path) { return tree_idx(v, path) >= 0; }

char **vfs_listdir(VFS *v, const char *path, int *cnt) {
    char **e = NULL; int n = 0;
    for (int i = 0; i < v->tree.count; i++)
        e = realloc(e, (size_t)(n+1) * sizeof(char *)), e[n++] = strdup(v->tree.paths[i]);
    *cnt = n; return e;
}

void vfs_listdir_free(char **e, int n) {
    for (int i = 0; i < n; i++) free(e[i]); free(e);
}

int vfs_stat(VFS *v, const char *path, VFS_Stat *st) {
    memset(st, 0, sizeof(*st));
    int i = tree_idx(v, path);
    if (i < 0) { st->exists = false; return 0; }
    st->exists = true;
    strncpy(st->sha, v->tree.shas[i], 64);
    uint8_t *d = NULL; size_t dl = 0;
    if (cas_read(v->cas, v->tree.shas[i], &d, &dl)) {
        size_t x; uint8_t *in = blob_read(d, dl, &x);
        st->size = (off_t)x;
        free(in); free(d);
    }
    return 0;
}

char *vfs_commit(VFS *v) { return tree_build_json(v->tree.paths, v->tree.shas, v->tree.count); }
Tree *vfs_get_tree(VFS *v) { return &v->tree; }

int vfs_checkout(VFS *v, const char *sha) {
    Snapshot *s = snapshot_get(v->cas, sha);
    if (!s) { errno = ENOENT; return -1; }
    tree_free(v->tree);
    v->tree = tree_parse(s->tree_json);
    snapshot_free(s);
    return 0;
}

int vfs_mkdir(VFS *v, const char *p) { (void)v; (void)p; return 0; }
