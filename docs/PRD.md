# MiniNAS-C - 产品需求文档 (PRD)

> Git 风格增量快照 · 轻量级文件存储系统 · C 语言实现

**版本：** v1.0.0
**语言：** C11
**作者：** OpenClaw Agent
**日期：** 2026-03-31
**状态：** 开发完成

---

## 1. 产品概述

### 1.1 背景

在软件开发、测试、数据分析等场景中，经常需要：
- 对文件系统状态进行**版本快照**，便于回滚和比对
- **隔离存储实例**，在同一目录下管理多个独立项目
- 在测试环境中**模拟真实文件操作**，不污染真实文件系统
- **增量存储**，避免重复数据占用过多空间

现有方案的问题：
- **Git**：面向代码版本管理，语义复杂，学习成本高
- **Docker/VM**：重量级，资源占用大
- **Python 原型**：运行时依赖 Python 环境

### 1.2 核心定位

MiniNAS-C 是一个专为**测试/模拟/调试**场景设计的轻量级版本化文件系统，采用纯 C 语言实现，无外部依赖，可直接编译运行。

**关键词：** 零外部依赖、C11 标准库、Git 风格快照、命名空间隔离、可插拔后端

---

## 2. 功能需求

### 2.1 内容寻址存储（CAS）

**核心原理：** 所有数据块（Blob）按 SHA-256 内容哈希寻址，相同内容自动去重。

**存储格式：**
```
.minnas/objects/XX/YYYY...
  36/b2/36b28c6d...  →  "blob 12\0Hello World!"
```

**接口：**
```c
char *cas_store(CAS *cas, const uint8_t *data, size_t len);
int    cas_load(CAS *cas, const char *sha_hex, uint8_t **out, size_t *out_len);
bool   cas_exists(CAS *cas, const char *sha_hex);
int    cas_gc(CAS *cas, char **roots, int n, int *freed);
```

**验收标准：**
- [x] 相同内容只存储一份（SHA-256 去重）
- [x] 支持垃圾回收（Mark-Sweep）
- [x] 支持 Local/Memory 两种后端

---

### 2.2 增量快照

**核心原理：** 快照 = 文件路径 → Blob SHA 的映射表（Tree）。每次提交只记录变更文件。

```
snapshot_v1: {"/a.txt": "sha_a1", "/b.txt": "sha_b1"}
snapshot_v2: {"/a.txt": "sha_a1", "/b.txt": "sha_b2", "/c.txt": "sha_c1"}
              ↑ 未变化，引用不变         ↑ 变化 → 新 SHA
```

**接口：**
```c
Snapshot *snapshot_create(CAS *cas, const char *tree_json,
                         const char *message, const char *author,
                         const char *parent_sha);
Snapshot *snapshot_get(CAS *cas, const char *sha_hex);
Change  **snapshot_diff(CAS *cas, const char *sha1, const char *sha2, int *count);
```

**验收标准：**
- [x] 增量快照：未修改文件引用不变
- [x] 快照历史查询
- [x] 快照 diff（新增/修改/删除）
- [x] 快照 checkout（恢复到指定版本）

---

### 2.3 分支管理 + Reflog

**Ref 存储：**
```
.minnas/refs/heads/main     → "abc123...\n"
.minnas/refs/heads/feature  → "def456...\n"
.minnas/HEAD               → "ref: refs/heads/main"
```

**Reflog 格式：**
```
{old_sha} {new_sha} {action} {author} {timestamp} {message}
```

**接口：**
```c
int branchmgr_create_branch(BranchMgr *bm, const char *name, const char *sha);
int branchmgr_checkout(BranchMgr *bm, const char *name);
int branchmgr_delete_branch(BranchMgr *bm, const char *name);
char **branchmgr_list_branches(BranchMgr *bm, int *count);
ReflogEntry **branchmgr_get_reflog(BranchMgr *bm, int max_count, int *count);
```

**验收标准：**
- [x] 创建/删除/切换分支
- [x] Reflog 自动记录每次 HEAD 变动
- [x] Detached HEAD 支持

---

### 2.4 命名空间隔离

**目录结构：**
```
.minnas/
├── _current_ns            → "default"
├── objects/               ← 所有命名空间共享 CAS
├── namespaces/
│   ├── default/          ← 独立快照索引
│   │   ├── current_tree
│   │   └── snapshots/
│   └── proj1/            ← 独立命名空间
└── refs/                 ← 共享分支
```

**接口：**
```c
int nsmgr_create_namespace(NamespaceMgr *nm, const char *name);
int nsmgr_switch_namespace(NamespaceMgr *nm, const char *name);
char **nsmgr_list_namespaces(NamespaceMgr *nm, int *count);
```

**验收标准：**
- [x] 创建/切换/删除命名空间
- [x] 命名空间间数据完全隔离
- [x] 共享底层 CAS（节省空间）

---

### 2.5 虚拟文件系统（VFS）

**支持的操作：**

| 操作 | 模式 | 说明 |
|------|------|------|
| `vfs_open()` | r/w/a/r+/w+/a+ | 打开文件 |
| `vfs_read()` | — | 读取 n 字节 |
| `vfs_write()` | — | 写入数据 |
| `vfs_lseek()` | — | SEEK_SET/CUR/END |
| `vfs_truncate()` | — | 截断文件 |
| `vfs_close()` | — | 关闭并提交 CAS |
| `vfs_rm()` | — | 删除文件 |
| `vfs_listdir()` | — | 列出目录 |

**工作原理：**
1. `open(path, 'w')` → 创建内存缓冲区
2. `write(fd, data, n)` → 写入缓冲区
3. `close(fd)` → 缓冲区内容写入 CAS，更新树映射

**验收标准：**
- [x] 所有文件模式正常
- [x] Seek/Tell 精确定位
- [x] Truncate 正确截断
- [x] 关闭时自动提交 CAS

---

### 2.6 可插拔后端

| 后端 | 说明 | 适用场景 |
|------|------|---------|
| `LocalBackend` | 本地目录存储 | 持久化、团队共享 |
| `MemoryBackend` | 内存存储 | 测试、快速原型 |
| `RemoteBackend` | HTTP 远程存储 | 多端同步（规划中） |

**接口：**
```c
Backend *backend_local_create(const char *root_path);
Backend *backend_memory_create(void);
void backend_free(Backend *b);
```

---

## 3. 非功能需求

### 3.1 编译要求

```bash
gcc --version  >= 5.0  (支持 C11)
make           标准工具
可选: zlib     压缩支持（默认关闭）
```

**编译：**
```bash
make                    # 编译 minnas + 运行测试
gcc src/*.c -o minnas  # 直接编译
```

### 3.2 性能目标

| 指标 | 目标 |
|------|------|
| 单文件写入（MemoryBackend） | < 1ms |
| 单文件写入（LocalBackend） | < 5ms |
| 快照创建 | O(变更文件数) |
| 内容去重率 | 相同内容 ≥ 1次去重 |

### 3.3 约束与限制

- **单进程**：不支持多进程并发写入
- **单文件大小**：建议 < 100MB
- **路径规范**：POSIX 风格，以 `/` 开头
- **线程安全**：不支持（单线程设计）

---

## 4. 技术架构（C 语言特定）

### 4.1 内存所有权模型

| 类型 | 创建者 | 释放者 | 说明 |
|------|--------|--------|------|
| `char *` 返回值 | 底层函数 | 调用者 `free()` | 字符串等 |
| `uint8_t *` 返回值 | `cas_load` | 调用者 `free()` | 原始数据 |
| `struct Snapshot *` | `snapshot_create/get` | `snapshot_free()` | 快照对象 |
| `Repo *` | `repo_init/open` | `repo_free()` | 仓库句柄 |

### 4.2 错误处理

- 返回 `-1` 或 `NULL` 表示错误
- `errno` 设置为标准 POSIX 错误码
- `fprintf(stderr, ...)` 输出人类可读错误信息

### 4.3 数据结构

```c
// 快照
typedef struct {
    char sha[65];       // SHA-256 十六进制
    char *tree_json;     // JSON 序列化树
    char *message;       // 提交信息
    char *author;        // 作者
    time_t timestamp;    // 时间戳
    char *parent_sha;    // 父快照 SHA
} Snapshot;

// 树：文件路径到 Blob SHA 的映射
typedef struct {
    char **paths;        // 文件路径数组
    char **shas;         // 对应 SHA 数组
    int count;           // 条目数量
} Tree;

// 变化条目
typedef struct {
    char action;         // 'A'dd, 'D'elete, 'M'odify
    char *path;
    char *old_sha;
    char *new_sha;
} Change;
```

---

## 5. 版本规划

| 版本 | 目标 | 状态 |
|------|------|------|
| v1.0.0 | 核心功能完成（C11 + POSIX） | ✅ |
| v1.1.0 | FUSE 挂载支持 | 规划中 |
| v1.2.0 | RemoteBackend（HTTP） | 规划中 |
| v2.0.0 | 多进程并发支持 | 规划中 |
| v2.1.0 | 性能优化（大型仓库） | 规划中 |
