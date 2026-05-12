# 第六章：SQLite C/C++ API 详解

本章深入讲解 SQLite 常用 API，帮助你在实际项目中灵活运用。

## 6.1 API 分层结构

```
┌────────────────────────────────────────────┐
│           应用层（你的代码）                   │
├────────────────────────────────────────────┤
│  高层 API                                   │
│  sqlite3_exec()    ← 简单执行，适合 DDL      │
├────────────────────────────────────────────┤
│  核心 API（推荐使用）                         │
│  sqlite3_prepare_v2()  → 编译 SQL            │
│  sqlite3_bind_*()      → 绑定参数            │
│  sqlite3_step()        → 执行/取下一行        │
│  sqlite3_column_*()    → 读取列值            │
│  sqlite3_finalize()    → 释放语句            │
│  sqlite3_reset()       → 重置语句（复用）     │
├────────────────────────────────────────────┤
│  数据库管理 API                              │
│  sqlite3_open_v2()     → 打开（高级选项）     │
│  sqlite3_close_v2()    → 关闭               │
│  sqlite3_errmsg()      → 错误信息            │
│  sqlite3_errcode()     → 错误码              │
├────────────────────────────────────────────┤
│  扩展 API                                   │
│  sqlite3_create_function()  → 自定义函数     │
│  sqlite3_busy_handler()     → 忙等处理       │
│  sqlite3_backup_*()         → 在线备份       │
└────────────────────────────────────────────┘
```

## 6.2 数据库打开选项

`sqlite3_open_v2()` 提供比 `sqlite3_open()` 更多的控制：

```cpp
int sqlite3_open_v2(
    const char *filename,   // 数据库文件名
    sqlite3 **ppDb,         // 输出：数据库句柄
    int flags,              // 打开标志
    const char *zVfs        // VFS 名称（通常为 NULL）
);
```

常用标志：

| 标志 | 含义 |
|------|------|
| `SQLITE_OPEN_READONLY` | 只读打开 |
| `SQLITE_OPEN_READWRITE` | 读写打开 |
| `SQLITE_OPEN_CREATE` | 不存在时创建 |
| `SQLITE_OPEN_MEMORY` | 内存数据库 |
| `SQLITE_OPEN_NOMUTEX` | 多线程模式（无互斥锁） |
| `SQLITE_OPEN_FULLMUTEX` | 串行模式（完全互斥） |

## 6.3 BLOB 数据处理

嵌入式系统经常需要存储二进制数据（固件片段、图像、波形数据等）：

```cpp
// 写入 BLOB
sqlite3_bind_blob(stmt, col, data_ptr, data_size, SQLITE_TRANSIENT);

// 读取 BLOB
const void *blob = sqlite3_column_blob(stmt, col);
int blob_size = sqlite3_column_bytes(stmt, col);
```

## 6.4 自定义函数

SQLite 允许你用 C/C++ 编写自定义 SQL 函数：

```cpp
sqlite3_create_function(db, "函数名", 参数数量, 编码, 用户数据,
                        标量函数回调, 聚合步骤回调, 聚合结束回调);
```

## 6.5 忙等处理

当数据库被其他连接锁定时，SQLite 默认立即返回 `SQLITE_BUSY` 错误。你可以设置忙等超时：

```cpp
// 方法1：设置超时时间（毫秒）
sqlite3_busy_timeout(db, 5000);  // 等待最多 5 秒

// 方法2：自定义忙等处理回调
sqlite3_busy_handler(db, busy_callback, user_data);
```

## 6.6 示例代码

完整示例：[examples/ch06_api_detail.cpp](../examples/ch06_api_detail.cpp)

运行方式：

```bash
cd build && ./ch06_api_detail
```

## 6.7 本章小结

- `sqlite3_open_v2()` 提供更精细的数据库打开控制
- BLOB 处理让你可以存储任意二进制数据
- 自定义函数扩展了 SQL 的能力
- `sqlite3_busy_timeout()` 处理并发锁冲突
- 正确的错误处理和资源管理是编写健壮代码的关键

---

[⬅ 第五章：事务与并发控制](ch05_transaction.md) | [目录](../README.md) | [第七章：性能优化 ➡](ch07_performance.md)
