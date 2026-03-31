# MiniNAS-C

**Git 风格增量快照 · 轻量级文件存储系统 · C11 实现**

> 支持命名空间隔离、增量快照、分支管理、完整文件语义，可插拔存储后端。

[![Build](https://img.shields.io/badge/build-gcc%20%7C%20clang-brightgreen.svg)]()
[![Std](https://img.shields.io/badge/std-C11-blue.svg)]()
[![Deps](https://img.shields.io/badge/deps-zero-green.svg)]()

## 特性

| 特性 | 说明 |
|------|------|
| **增量快照** | Git 风格 CAS，只存储变更内容 |
| **命名空间隔离** | 多实例数据完全独立 |
| **分支管理** | 创建/切换/删除分支 + Reflog |
| **VFS 语义** | open/read/write/seek/append/truncate |
| **可插拔后端** | 本地目录 / 内存 / 远程 HTTP |
| **零依赖** | 仅 C11 标准库 + make |

## 快速上手

```bash
# 编译
make

# 初始化仓库
./minnas init --path ./testrepo

# 文件操作
./minnas fs write /hello.txt "Hello MiniNAS!"
./minnas commit "initial commit"
./minnas log
./minnas stats
./minnas gc
```

## 架构

```
┌─────────────────────────────────────────┐
│                  CLI                     │
└─────────────────────┬───────────────────┘
                      │
              ┌───────▼────────┐
              │      Repo       │
              └───────┬─────────┘
          ┌──────────┼──────────┐
          │          │          │
    ┌─────▼────┐ ┌───▼──┐ ┌───▼──────────┐
    │   VFS    │ │Branch│ │ SnapshotStore│
    │ 虚拟文件系统│ │ Manager│ │  (CAS引擎)   │
    └─────┬────┘ └──┬──┘ └──────┬───────┘
          │          │            │
    ┌─────▼─────────▼──┐  ┌─────▼───────┐
    │   Namespace      │  │   Backend   │
    │   Manager        │  │ (Local/Mem) │
    └─────────────────┘  └─────────────┘
```

## 命令参考

| 命令 | 说明 |
|------|------|
| `minnas init` | 初始化仓库 |
| `minnas status` | 仓库状态 |
| `minnas commit <msg>` | 创建快照 |
| `minnas log [-n N]` | 查看历史 |
| `minnas snapshot list` | 列出快照 |
| `minnas snapshot show <sha>` | 快照详情 |
| `minnas branch` | 列出分支 |
| `minnas branch create <name>` | 创建分支 |
| `minnas fs ls [path]` | 列出目录 |
| `minnas fs write <p> <c>` | 写入文件 |
| `minnas fs cat <path>` | 读取文件 |
| `minnas gc` | 垃圾回收 |
| `minnas stats` | 统计信息 |

## 编译

```bash
# Linux / macOS
make
# 或者直接编译
gcc src/*.c -o minnas -lm -Wall -std=c11 -g
```

## 测试

```bash
make test
```

## License

MIT
