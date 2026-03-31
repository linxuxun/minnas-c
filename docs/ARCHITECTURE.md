# MiniNAS-C - 架构设计文档

> 深入解析 MiniNAS-C 的核心设计与 C 语言实现思路

**版本：** v1.0.0
**目标读者：** 开发者 / 架构师
**语言：** C11

---

## 1. 设计原则

1. **内容寻址优于路径寻址** — SHA-256 哈希去重，相同内容天然复用
2. **快照是不可变事实** — 历史快照一旦创建不可修改，只有 HEAD 可移动
3. **命名空间天然隔离** — 共享底层 CAS，独立快照索引
4. **零外部依赖** — 仅 C11 标准库 + make，任何环境开箱即用
5. **显式内存管理** — 谁分配谁释放，无 GC，无隐式生命周期

---

## 2. 系统架构

```
┌──────────────────────────────────────────────────────┐
│                   CLI 层 (cli.c)                     │
│         彩色输出 / 参数解析 / 命令路由                 │
└──────────────────────┬───────────────────────────────┘
                        │
               ┌────────▼────────┐
               │   Repo (minnas.c) │
               │  统一入口接口   │
               └─────┬───────────┘
         ┌──────────┼──────────┐
         │          │          │
  ┌──────▼──┐ ┌───▼────┐ ┌───▼─────────┐
  │   VFS   │ │ Branch │ │ SnapshotStore │
  │ 虚拟文件系统 │ │ Manager │ │   (CAS引擎)   │
  └────┬────┘ └───┬────┘ └──────┬──────┘
       │          │              │
  ┌────▼─────────▼──┐  ┌─────▼──────┐
  │   Namespace     │  │   Backend   │
  │   Manager       │  │ Local/Mem   │
  └─────────────────┘  └────────────┘
```

---

## 3. 核心数据结构

### 3.1 Blob（数据块）

**存储格式：**
```
"blob {size}\0{data...}"
 ↑ header        ↑ content
```

**代码实现：**
```c
uint8_t *blob_build(const uint8_t *data, size_t data_len, size_t *out_len) {
    char header[32];
    int hlen = snprintf(header, sizeof(header), "blob %zu\0", data_len);
    size_t blob_len = (size_t)hlen + data_len;
    uint8_t *blob = malloc(blob_len);
    memcpy(blob, header, (size_t)hlen);
    memcpy(blob + hlen, data, data_len);
    *out_len = blob_len;
    return blob;
}
```

**读取：**
```c
uint8_t *blob_read(const uint8_t *blob, size_t blob_len, size_t *out_data_len) {
    // 解析 header 中的 size 字段
    // 验证 blob_len == header_size + data_size
    // 返回数据副本（调用者 free）
}
```

---

### 3.2 CAS 引擎（Content-Addressable Store）

**存储路径：**
```
{minnas_root}/objects/{sha[0:2]}/{sha[2:]}
例如：objects/36/b2/36b28c6d...  →  blob 二进制内容
```

**核心 API：**
```c
// 存储数据，返回 SHA-256 十六进制字符串（调用者 free）
char *cas_store(CAS *cas, const uint8_t *data, size_t len) {
    uint8_t digest[32];
    sha256_hash(data, len, digest);         // SHA-256 计算
    char sha_hex[65];
    sha256_hex_to(sha_hex, digest);        // 转换为十六进制字符串

    uint8_t *blob = blob_build(data, len, &blob_len);
    backend->ops->write(ctx, sha_hex, blob, blob_len);
    free(blob);
    return strdup(sha_hex);                // caller must free
}
```

**GC（垃圾回收）：**
```c
int cas_gc(CAS *cas, char **roots, int root_count, int *freed_count) {
    bool reachable[OBJECT_COUNT] = {false};  // Mark
    for each root_sha in roots:
        traverse_and_mark(snap, reachable);   // BFS 遍历可达对象

    int freed = 0;
    for each object in store:
        if (!reachable[i])
            backend->ops->delete(object_sha), freed++;
    *freed_count = freed;
}
```

---

### 3.3 Snapshot（快照）

**JSON 表示：**
```json
{
  "tree": {"readme.txt": "sha_a1", "config.py": "sha_b2"},
  "message": "update config",
  "author": "alice",
  "timestamp": 1743400000,
  "parent": "sha_parent_or_null"
}
```

**存储方式：** 快照 JSON 本身被打包成 Blob，通过 CAS 存储。

---

### 3.4 Tree（文件映射）

**结构：**
```c
typedef struct {
    char **paths;   // [n] 文件路径，如 "src/main.c"
    char **shas;    // [n] 对应 Blob SHA
    int count;      // 条目数量
} Tree;
```

**构建：**
```c
char *tree_build_json(char **paths, char **shas, int count) {
    // 构建 {"path1":"sha1","path2":"sha2",...}
    // 手动 JSON 序列化，无外部库依赖
}
```

---

## 4. 模块详解

### 4.1 VirtualFS（虚拟文件系统）

**核心数据结构：**
```c
typedef struct VFile {
    char *path;           // 文件路径
    char mode;            // 'r' 'w' 'a'
    bool modified;         // 是否有未提交修改
    char *blob_sha;       // 原始 Blob SHA
    uint8_t *buffer;     // 内存缓冲区
    size_t buf_size;      // 当前数据大小
    size_t buf_cap;       // 缓冲区容量
    size_t position;      // 读写指针位置
    bool closed;
} VFile;

typedef struct VFS {
    CAS *cas;
    Tree tree;              // 当前工作树
    VFile *files[128];       // fd → VFile 映射
    int next_fd;             // 下一个可用 fd
} VFS;
```

**文件操作状态机：**
```
open(path, 'w')
    ↓
申请 VFile，分配缓冲区
    ↓
write(fd, buf, n)
    ↓
追加到缓冲区，标记 modified=true
    ↓
close(fd)
    ↓
blob = blob_build(buffer, buf_size)
sha  = cas_store(blob)
tree[path] = sha
```

---

### 4.2 BranchManager（分支管理）

**Ref 文件格式：**
```
{minnas_root}/refs/heads/{branch_name}
  内容：{sha_hex}\n
```

**HEAD 文件格式：**
```
ref: refs/heads/main     ← 符号引用
sha_hex_string           ← Detached HEAD
```

**Reflog 格式：**
```
{old_sha} {new_sha} {action} {author} {timestamp_unix} {message}\n
abc123... def456... commit alice 1743400000 update config
```

---

### 4.3 NamespaceManager（命名空间隔离）

**设计决策：所有命名空间共享同一个 objects/ 目录**

优点：
- 相同内容跨命名空间复用（节省存储）
- GC 只需扫描一次
- 快照可跨命名空间引用

缺点：
- 删除命名空间时不能直接删除对象（需要 GC）

---

### 4.4 Backend（可插拔后端）

```c
typedef struct BackendOps {
    int (*write)(void *ctx, const char *sha_hex, const uint8_t *data, size_t len);
    int (*read)(void *ctx, const char *sha_hex, uint8_t **out_data, size_t *out_len);
    int (*exists)(void *ctx, const char *sha_hex);
    int (*delete)(void *ctx, const char *sha_hex);
    char** (*list_all)(void *ctx, int *count);
    void (*free)(void *ctx);
} BackendOps;

typedef struct Backend {
    const BackendOps *ops;
    void *ctx;   // 后端具体实现的数据
    CAS *cas;    // 回指 CAS（GC 时需要）
} Backend;
```

**LocalBackend 实现：**
```c
static int local_write(void *ctx, const char *sha, const uint8_t *d, size_t l) {
    LocalCtx *c = ctx;
    char path[128];
    snprintf(path, sizeof(path), "%s/objects/%s/%s",
             c->root, sha_subdir(sha), sha + 2);
    ensure_parent_dir(path);
    return write_file_atomic(path, d, l);  // write + rename 原子写入
}
```

---

## 5. 内存管理模型

### 5.1 所有权规则

```
┌─────────────────────────────────────────────────────────┐
│  规则1：函数返回的指针 = 调用者负责 free                  │
│  规则2：struct 中的指针 = struct 的 free 函数负责           │
│  规则3：FILE* = fopen/fclose 配对                        │
│  规则4：fd = open/close 配对（VFS 中）                  │
└─────────────────────────────────────────────────────────┘
```

### 5.2 free 函数一览

| 函数 | 释放对象 | 说明 |
|------|---------|------|
| `free()` | `char *`, `uint8_t *` | 标准 C |
| `snapshot_free()` | `Snapshot *` | 释放子字符串 |
| `tree_free()` | `Tree.paths/shas` | 释放数组 |
| `changes_free()` | `Change[]` | 释放数组 |
| `reflog_free()` | `ReflogEntry[]` | 释放数组 |
| `repo_free()` | `Repo` 及所有子模块 | 顶层释放 |

---

## 6. 错误处理规范

```c
// 错误返回规范
int cas_write(...) { return -1; }       // 失败
int cas_write(...) { return 0; }         // 成功
// 成功返回 0，失败返回 -1（POSIX 惯例）

// 设置 errno
errno = ENOENT;   // 文件不存在
errno = EINVAL;    // 无效参数
errno = EMFILE;    // fd 耗尽

// 人类可读错误（stderr）
fprintf(stderr, C_RED "✗" C_RST " error: %s\n", strerror(errno));
```

---

## 7. 关键设计决策

### Q1: 为什么用 SHA-256 而不是 CRC32？

SHA-256 碰撞概率极低（2^-128），且与 Git 兼容。CRC32 碰撞率高，不适合内容寻址。

### Q2: 为什么不用外部 JSON 库？

手动实现简单的 JSON 序列化（字典结构固定为字符串→字符串映射），无正则、无转义复杂度，可完整控制内存分配。

### Q3: 为什么所有路径都是 POSIX 风格？

C 语言本身无原生路径规范，POSIX 是最广泛支持的约定。所有路径以 `/` 分隔。

### Q4: 为什么 LocalBackend 用 write+rename 而非直接 write？

原子性保证：写入时系统崩溃，只会留下 `.tmp` 文件，不会留下不完整的 blob 文件。下次 GC 时清理。

### Q5: 为什么 VFS 用固定数量 fd 表而不是动态分配？

简化实现，128 个并发文件描述符对目标场景足够。超限返回 EMFILE。
