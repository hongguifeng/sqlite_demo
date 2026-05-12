# 第二章：SQLite 简介与环境搭建

## 2.1 什么是 SQLite？

SQLite 是一个**嵌入式关系型数据库引擎**。与 MySQL、PostgreSQL 等"客户端-服务器"架构的数据库不同，SQLite 具有以下显著特点：

### 核心特性

| 特性 | 说明 | 对嵌入式工程师的意义 |
|------|------|---------------------|
| **零配置** | 无需安装、无需启动服务 | 不占用额外系统资源 |
| **单文件存储** | 整个数据库就是一个 `.db` 文件 | 可以直接拷贝/备份 |
| **进程内运行** | 以库的形式链接到你的程序中 | 无 IPC 开销，调用就像调用函数 |
| **跨平台** | 支持 Linux/Windows/macOS/嵌入式 | 一份代码到处运行 |
| **轻量** | 编译后约 300-700KB | 适合资源受限系统 |
| **公有领域** | 无版权、无 License 限制 | 可自由用于商业产品 |

### 架构对比

```
传统数据库（如 MySQL）：            SQLite：

┌──────────┐                    ┌──────────────────┐
│ 你的应用程序 │                    │   你的应用程序      │
└─────┬────┘                    │  ┌────────────┐  │
      │ 网络/Socket              │  │ SQLite 库   │  │
┌─────┴────┐                    │  └──────┬─────┘  │
│ 数据库服务器 │                    └────────┬─────────┘
│  (独立进程) │                             │ 直接文件 I/O
└─────┬────┘                    ┌──────────┴──────┐
      │                        │  database.db 文件 │
┌─────┴────┐                    └─────────────────┘
│ 数据库文件  │
└──────────┘
```

### SQLite 的应用场景

SQLite 是世界上部署最广泛的数据库，你可能每天都在使用它：

- **Android/iOS**：每个 App 的本地数据存储
- **浏览器**：Chrome/Firefox 的书签、历史记录
- **嵌入式设备**：IoT 网关的数据记录、车载系统的配置管理
- **桌面软件**：邮件客户端、音乐播放器的元数据管理

## 2.2 环境搭建

### 安装 SQLite 开发库

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install libsqlite3-dev sqlite3

# 验证安装
sqlite3 --version
pkg-config --libs sqlite3
```

### 项目结构

本教程使用 CMake 构建，项目根目录的 `CMakeLists.txt` 已配置好所有示例的编译规则。

### 构建与运行

```bash
cd sqlite_demo
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行本章示例
./ch02_hello_sqlite
```

## 2.3 第一个 SQLite 程序

让我们写一个最简单的程序：打开（创建）一个数据库文件，然后关闭它。

### 示例代码

文件：`examples/ch02_hello_sqlite.cpp`

```cpp
#include <sqlite3.h>
#include <cstdio>

int main() {
    sqlite3 *db = nullptr;

    // 打开（或创建）数据库文件
    // 如果 test.db 不存在，SQLite 会自动创建它
    int rc = sqlite3_open("test.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("成功打开数据库！\n");
    printf("SQLite 版本: %s\n", sqlite3_libversion());

    // 使用 sqlite3_exec 执行一条简单的 SQL 查看版本
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(db, "SELECT sqlite_version();", -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        printf("SQL 查询返回的版本: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // 关闭数据库
    sqlite3_close(db);
    printf("数据库已关闭。\n");

    // 清理测试文件
    remove("test.db");

    return 0;
}
```

### 代码解析

这个简短的程序包含了 SQLite C API 的基本使用模式：

#### 1. 包含头文件

```cpp
#include <sqlite3.h>
```

这是 SQLite 的唯一头文件，包含了所有需要的函数声明和常量定义。

#### 2. 打开数据库

```cpp
sqlite3 *db = nullptr;
int rc = sqlite3_open("test.db", &db);
```

- `sqlite3` 是数据库连接的不透明指针类型（类似嵌入式中的外设句柄）
- `sqlite3_open()` 打开或创建一个数据库文件
- 返回 `SQLITE_OK`（值为 0）表示成功，其他值表示错误

#### 3. 错误处理

```cpp
if (rc != SQLITE_OK) {
    fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
}
```

- `sqlite3_errmsg()` 返回最近一次错误的描述字符串
- 即使打开失败，也需要调用 `sqlite3_close()` 来释放资源

#### 4. 关闭数据库

```cpp
sqlite3_close(db);
```

- **必须在程序退出前关闭数据库**，否则可能导致数据丢失
- 类似于嵌入式中关闭文件描述符或释放外设资源

### API 使用模式总结

```
sqlite3_open()    →  获得数据库句柄
    │
    ├── sqlite3_prepare_v2()  →  编译 SQL 语句
    │       │
    │       ├── sqlite3_step()  →  执行/获取下一行
    │       │
    │       └── sqlite3_finalize()  →  释放编译好的语句
    │
    └── sqlite3_close()  →  关闭数据库
```

这个模式和嵌入式开发中的资源管理非常类似：
- `open` → `use` → `close`
- 就像 UART：`uart_init()` → `uart_read/write()` → `uart_deinit()`

## 2.4 使用 sqlite3 命令行工具

SQLite 附带一个命令行工具 `sqlite3`，可以交互式地操作数据库，非常适合学习和调试：

```bash
# 创建/打开一个数据库
sqlite3 mytest.db

# 在 sqlite3 shell 中执行：
sqlite> .help                    -- 查看帮助
sqlite> .tables                  -- 列出所有表
sqlite> SELECT sqlite_version(); -- 查看版本
sqlite> .quit                    -- 退出
```

这个工具在调试时很有用——你可以直接打开程序生成的 `.db` 文件来检查数据是否正确。

## 2.5 本章小结

- SQLite 是一个零配置、单文件、进程内的嵌入式数据库
- 使用 `sqlite3_open()` / `sqlite3_close()` 管理数据库连接
- 所有 SQLite API 返回整型错误码，`SQLITE_OK` 表示成功
- 资源管理模式与嵌入式外设操作类似

---

[⬅ 第一章：数据库基础概念](ch01_database_basics.md) | [目录](../README.md) | [第三章：SQL 基本操作 ➡](ch03_basic_sql.md)
