# SQLite 教程 —— 面向嵌入式软件开发工程师

本教程专为有一定软件开发基础、但对数据库和 SQLite 没有了解的嵌入式软件开发工程师编写。从数据库基本概念讲起，逐步深入到 SQL 操作、C++ API 使用、性能调优，直至完整的嵌入式实战项目。

每个知识点都配有可编译运行的 C++ 示例代码。

---

## 快速开始

```bash
# 1. 安装依赖（Ubuntu/Debian）
sudo apt install build-essential cmake libsqlite3-dev

# 2. 构建所有示例
mkdir build && cd build
cmake ..
make -j$(nproc)

# 3. 运行示例
./ch02_hello_sqlite
```

> 其他平台：macOS 可通过 `brew install sqlite3` 安装，Windows 建议使用 WSL。

---

## 目录

| 章节 | 主题 | 示例程序 | 难度 |
|:----:|------|----------|:----:|
| 1 | [数据库基础概念](docs/ch01_database_basics.md) | — | ⭐ |
| 2 | [SQLite 简介与环境搭建](docs/ch02_sqlite_intro.md) | `ch02_hello_sqlite` | ⭐ |
| 3 | [SQL 基本操作（CRUD）](docs/ch03_basic_sql.md) | `ch03_create_table` `ch03_insert_data` `ch03_query_data` `ch03_update_delete` | ⭐⭐ |
| 4 | [高级查询](docs/ch04_advanced_query.md) | `ch04_advanced_query` | ⭐⭐⭐ |
| 5 | [事务与并发控制](docs/ch05_transaction.md) | `ch05_transaction` | ⭐⭐⭐ |
| 6 | [C/C++ API 详解](docs/ch06_api_detail.md) | `ch06_api_detail` | ⭐⭐⭐ |
| 7 | [性能优化与最佳实践](docs/ch07_performance.md) | `ch07_performance` | ⭐⭐⭐ |
| 8 | [嵌入式场景实战](docs/ch08_embedded.md) | `ch08_embedded` | ⭐⭐⭐⭐ |

---

## 推荐学习路径

**入门（1 ~ 3 天）** — 第 1 ~ 3 章：理解数据库概念，掌握 CRUD 操作

**进阶（1 ~ 2 周）** — 第 4 ~ 6 章：学会 JOIN / 索引 / 事务 / 高级 API

**实战（1 ~ 2 周）** — 第 7 ~ 8 章：性能调优，完成一个完整的传感器数据管理系统

---

## 环境要求

| 依赖 | 最低版本 | 说明 |
|------|---------|------|
| GCC / G++ | 7+ | 需支持 C++17 |
| CMake | 3.14+ | 构建系统 |
| libsqlite3-dev | 3.x | SQLite 头文件与链接库 |

---

## 项目结构

```
sqlite_demo/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
├── docs/                       # 教程文档（8 章 Markdown）
│   ├── ch01_database_basics.md
│   ├── ch02_sqlite_intro.md
│   ├── ch03_basic_sql.md
│   ├── ch04_advanced_query.md
│   ├── ch05_transaction.md
│   ├── ch06_api_detail.md
│   ├── ch07_performance.md
│   └── ch08_embedded.md
└── examples/                   # 示例代码（10 个 C++ 程序）
    ├── ch02_hello_sqlite.cpp
    ├── ch03_create_table.cpp
    ├── ch03_insert_data.cpp
    ├── ch03_query_data.cpp
    ├── ch03_update_delete.cpp
    ├── ch04_advanced_query.cpp
    ├── ch05_transaction.cpp
    ├── ch06_api_detail.cpp
    ├── ch07_performance.cpp
    └── ch08_embedded.cpp
```
