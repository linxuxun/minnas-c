#ifndef MINNAS_VFS_H
#define MINNAS_VFS_H

#include "cas.h"
#include "snapshot.h"
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct VFS VFS;

typedef struct VFS_Stat {
    bool exists;
    off_t size;
    char sha[65];
} VFS_Stat;

VFS *vfs_create(CAS *cas, Tree *initial_tree);
void vfs_free(VFS *vfs);

// Open modes: "r", "w", "a", "r+", "w+", "a+"
// Returns fd (>=0) or -1 on error (sets errno).
int vfs_open(VFS *vfs, const char *path, const char *mode);
int vfs_close(VFS *vfs, int fd);
int vfs_read(VFS *vfs, int fd, uint8_t *buf, int n);
int vfs_write(VFS *vfs, int fd, const uint8_t *buf, int n);
int64_t vfs_lseek(VFS *vfs, int fd, int64_t offset, int whence);
int64_t vfs_tell(VFS *vfs, int fd);
int vfs_truncate(VFS *vfs, const char *path, off_t size);

// Directory operations
int vfs_mkdir(VFS *vfs, const char *path);
int vfs_rm(VFS *vfs, const char *path);
bool vfs_exists(VFS *vfs, const char *path);
char **vfs_listdir(VFS *vfs, const char *path, int *count);
void vfs_listdir_free(char **entries, int count);
int vfs_stat(VFS *vfs, const char *path, VFS_Stat *st);

// Commit all modified files to CAS, returns new tree JSON (caller frees).
char *vfs_commit(VFS *vfs);

// Get current working tree (pointer, not copy - do not free).
Tree *vfs_get_tree(VFS *vfs);

// Checkout snapshot to VFS working tree.
int vfs_checkout(VFS *vfs, const char *snapshot_sha);

// Error string
const char *vfs_error(VFS *vfs);

#endif // MINNAS_VFS_H
