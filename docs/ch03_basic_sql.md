# 第三章：SQL 基本操作（CRUD）

本章是 SQL 学习的核心，将详细讲解创建表、插入、查询、更新和删除操作。每个操作都有对应的 C++ 示例代码。

## 3.1 创建表（CREATE TABLE）

### SQL 语法

```sql
CREATE TABLE 表名 (
    列名1 数据类型 [约束],
    列名2 数据类型 [约束],
    ...
);
```

### SQLite 数据类型

SQLite 的类型系统与传统数据库不同，它使用**类型亲和性（Type Affinity）**：

| 亲和类型 | 说明 | C/C++ 对应 |
|----------|------|-----------|
| INTEGER  | 整数 (1/2/4/8字节自适应) | `int`, `int64_t` |
| REAL     | 浮点数 (8字节 IEEE 754) | `double` |
| TEXT     | 字符串 (UTF-8) | `const char*` |
| BLOB     | 二进制数据 | `void*` + 长度 |
| NULL     | 空值 | `nullptr` |

> **嵌入式工程师注意**：SQLite 的类型是灵活的。与 C 语言不同，你可以在 INTEGER 列中存入字符串（虽然不建议这样做）。SQLite 会做隐式类型转换。

### 常用约束

| 约束 | 说明 | 类比 |
|------|------|------|
| `PRIMARY KEY` | 主键，唯一标识每一行 | 数组下标 |
| `NOT NULL` | 不允许为空 | 必须初始化的变量 |
| `UNIQUE` | 值不能重复 | 唯一 ID |
| `DEFAULT value` | 默认值 | 变量初始值 |
| `AUTOINCREMENT` | 自动递增（仅 INTEGER PRIMARY KEY） | 自增计数器 |

### 示例代码

文件：`examples/ch03_create_table.cpp`

```cpp
#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>

// 辅助函数：执行 SQL 并检查错误
static bool exec_sql(sqlite3 *db, const char *sql, const char *desc) {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[%s] 失败: %s\n", desc, err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    printf("[%s] 成功\n", desc);
    return true;
}

int main() {
    sqlite3 *db = nullptr;
    int rc = sqlite3_open("create_table_demo.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // 1. 创建设备表
    const char *sql_create_devices =
        "CREATE TABLE IF NOT EXISTS devices ("
        "    device_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name        TEXT NOT NULL,"
        "    type        TEXT NOT NULL DEFAULT 'sensor',"
        "    ip_address  TEXT,"
        "    created_at  TEXT DEFAULT (datetime('now', 'localtime'))"
        ");";
    exec_sql(db, sql_create_devices, "创建 devices 表");

    // 2. 创建传感器数据表（带外键关联）
    const char *sql_create_sensor_data =
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "    id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    device_id   INTEGER NOT NULL,"
        "    temperature REAL,"
        "    humidity    REAL,"
        "    timestamp   TEXT DEFAULT (datetime('now', 'localtime')),"
        "    FOREIGN KEY (device_id) REFERENCES devices(device_id)"
        ");";
    exec_sql(db, sql_create_sensor_data, "创建 sensor_data 表");

    // 3. 查看已创建的表
    printf("\n--- 数据库中的表 ---\n");
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(db,
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;",
        -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  表: %s\n", sqlite3_column_text(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);

    // 4. 查看 devices 表的结构
    printf("\n--- devices 表结构 ---\n");
    rc = sqlite3_prepare_v2(db,
        "PRAGMA table_info(devices);", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  列: %-15s 类型: %-10s 非空: %d 默认: %s\n",
                sqlite3_column_text(stmt, 1),   // name
                sqlite3_column_text(stmt, 2),   // type
                sqlite3_column_int(stmt, 3),     // notnull
                sqlite3_column_text(stmt, 4) ?
                    (const char*)sqlite3_column_text(stmt, 4) : "(无)");
        }
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    remove("create_table_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
```

### 代码要点

1. **`sqlite3_exec()`**：执行不需要返回数据的 SQL（DDL语句如 CREATE TABLE）
2. **`IF NOT EXISTS`**：防止重复创建表时报错
3. **`sqlite_master`**：SQLite 内部的系统表，记录了数据库中所有的表、索引等
4. **`PRAGMA table_info()`**：查看表结构的特殊命令

---

## 3.2 插入数据（INSERT）

### SQL 语法

```sql
-- 指定列名插入
INSERT INTO 表名 (列1, 列2, ...) VALUES (值1, 值2, ...);

-- 插入多行
INSERT INTO 表名 (列1, 列2) VALUES (值1a, 值2a), (值1b, 值2b);
```

### 参数绑定（防止 SQL 注入）

> ⚠️ **安全警告**：永远不要用字符串拼接来构造 SQL！这会导致 **SQL 注入**攻击。

```cpp
// ❌ 危险：字符串拼接
char sql[256];
sprintf(sql, "INSERT INTO devices (name) VALUES ('%s');", user_input);
// 如果 user_input = "'); DROP TABLE devices; --"，你的表就没了！

// ✅ 安全：参数绑定
sqlite3_prepare_v2(db, "INSERT INTO devices (name) VALUES (?);", ...);
sqlite3_bind_text(stmt, 1, user_input, -1, SQLITE_TRANSIENT);
```

### 示例代码

文件：`examples/ch03_insert_data.cpp`

```cpp
#include <sqlite3.h>
#include <cstdio>
#include <cstring>

static bool exec_sql(sqlite3 *db, const char *sql) {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("insert_demo.db", &db);

    // 创建表
    exec_sql(db,
        "CREATE TABLE IF NOT EXISTS devices ("
        "  device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  type TEXT NOT NULL DEFAULT 'sensor',"
        "  ip_address TEXT"
        ");");

    exec_sql(db,
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id INTEGER NOT NULL,"
        "  temperature REAL,"
        "  humidity REAL,"
        "  timestamp TEXT DEFAULT (datetime('now','localtime')),"
        "  FOREIGN KEY(device_id) REFERENCES devices(device_id)"
        ");");

    // ==========================================
    // 方法1：使用 sqlite3_exec 直接执行（简单但不安全）
    // ==========================================
    printf("--- 方法1: 直接执行 SQL ---\n");
    exec_sql(db,
        "INSERT INTO devices (name, type, ip_address) "
        "VALUES ('温度传感器A', 'sensor', '192.168.1.101');");
    printf("插入设备 1，rowid = %lld\n", sqlite3_last_insert_rowid(db));

    // ==========================================
    // 方法2：使用预编译语句 + 参数绑定（推荐）
    // ==========================================
    printf("\n--- 方法2: 预编译语句 + 参数绑定 ---\n");

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "INSERT INTO devices (name, type, ip_address) VALUES (?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "预编译失败: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    // 绑定参数并执行 —— 设备2
    sqlite3_bind_text(stmt, 1, "温度传感器B", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "sensor", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "192.168.1.102", -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        printf("插入设备 2，rowid = %lld\n", sqlite3_last_insert_rowid(db));
    }

    // 重置语句，复用来插入下一条
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    // 绑定参数并执行 —— 设备3
    sqlite3_bind_text(stmt, 1, "网关设备", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "gateway", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "192.168.1.1", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    printf("插入设备 3，rowid = %lld\n", sqlite3_last_insert_rowid(db));

    sqlite3_finalize(stmt);

    // ==========================================
    // 批量插入传感器数据
    // ==========================================
    printf("\n--- 批量插入传感器数据 ---\n");

    const char *sql_sensor =
        "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);";
    sqlite3_prepare_v2(db, sql_sensor, -1, &stmt, nullptr);

    // 模拟 10 条传感器数据
    struct SensorReading {
        int device_id;
        double temperature;
        double humidity;
    };

    SensorReading readings[] = {
        {1, 25.3, 60.2}, {1, 25.5, 59.8}, {1, 25.1, 61.0},
        {2, 24.1, 62.0}, {2, 24.3, 61.5}, {2, 24.0, 62.3},
        {1, 25.7, 59.5}, {2, 24.5, 61.8}, {1, 25.9, 59.0},
        {2, 24.2, 62.1}
    };

    for (const auto &r : readings) {
        sqlite3_bind_int(stmt, 1, r.device_id);
        sqlite3_bind_double(stmt, 2, r.temperature);
        sqlite3_bind_double(stmt, 3, r.humidity);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    printf("已插入 %d 条传感器数据\n", (int)(sizeof(readings)/sizeof(readings[0])));

    // 验证：查询总数
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sensor_data;", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("sensor_data 表共 %d 条记录\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    remove("insert_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
```

### 绑定函数一览

| 函数 | 绑定类型 | 对应 C 类型 |
|------|----------|------------|
| `sqlite3_bind_int()` | INTEGER | `int` |
| `sqlite3_bind_int64()` | INTEGER | `int64_t` |
| `sqlite3_bind_double()` | REAL | `double` |
| `sqlite3_bind_text()` | TEXT | `const char*` |
| `sqlite3_bind_blob()` | BLOB | `const void*` |
| `sqlite3_bind_null()` | NULL | - |

> `SQLITE_TRANSIENT` 告诉 SQLite 立刻复制字符串内容。如果你的字符串是静态的或生命周期足够长，可以用 `SQLITE_STATIC` 避免复制。

---

## 3.3 查询数据（SELECT）

### SQL 语法

```sql
SELECT 列1, 列2 FROM 表名 [WHERE 条件] [ORDER BY 列 ASC|DESC] [LIMIT 数量];

-- 查询所有列
SELECT * FROM devices;

-- 带条件查询
SELECT name, temperature FROM sensor_data WHERE temperature > 25.0;

-- 排序 + 限制数量
SELECT * FROM sensor_data ORDER BY temperature DESC LIMIT 5;
```

### 示例代码

文件：`examples/ch03_query_data.cpp`

```cpp
#include <sqlite3.h>
#include <cstdio>

static bool exec_sql(sqlite3 *db, const char *sql) {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

static void prepare_test_data(sqlite3 *db) {
    exec_sql(db,
        "CREATE TABLE IF NOT EXISTS devices ("
        "  device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL, type TEXT NOT NULL, ip_address TEXT);");
    exec_sql(db,
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id INTEGER NOT NULL,"
        "  temperature REAL, humidity REAL,"
        "  timestamp TEXT DEFAULT (datetime('now','localtime')));");
    exec_sql(db,
        "INSERT INTO devices (name, type, ip_address) VALUES "
        "('温度传感器A', 'sensor', '192.168.1.101'),"
        "('温度传感器B', 'sensor', '192.168.1.102'),"
        "('网关设备', 'gateway', '192.168.1.1');");
    exec_sql(db,
        "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES "
        "(1, 25.3, 60.2),(1, 25.5, 59.8),(1, 25.1, 61.0),"
        "(2, 24.1, 62.0),(2, 24.3, 61.5),(2, 24.0, 62.3),"
        "(1, 26.7, 58.5),(2, 23.5, 63.8),(1, 27.1, 57.0);");
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("query_demo.db", &db);
    prepare_test_data(db);

    sqlite3_stmt *stmt = nullptr;

    // ==========================================
    // 1. 查询所有设备
    // ==========================================
    printf("=== 1. 所有设备 ===\n");
    sqlite3_prepare_v2(db, "SELECT device_id, name, type, ip_address FROM devices;",
                       -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  ID=%d  名称=%-12s  类型=%-8s  IP=%s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "N/A");
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 2. 条件查询：温度 > 25 度的记录
    // ==========================================
    printf("\n=== 2. 温度 > 25°C 的记录 ===\n");
    sqlite3_prepare_v2(db,
        "SELECT id, device_id, temperature, humidity FROM sensor_data "
        "WHERE temperature > ?;", -1, &stmt, nullptr);
    sqlite3_bind_double(stmt, 1, 25.0);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  ID=%d  设备=%d  温度=%.1f°C  湿度=%.1f%%\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 3. 排序查询：按温度降序排列，取前3条
    // ==========================================
    printf("\n=== 3. 温度最高的前3条记录 ===\n");
    sqlite3_prepare_v2(db,
        "SELECT device_id, temperature FROM sensor_data "
        "ORDER BY temperature DESC LIMIT 3;", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  设备=%d  温度=%.1f°C\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 4. 聚合查询：每个设备的平均温度
    // ==========================================
    printf("\n=== 4. 每个设备的平均温度 ===\n");
    sqlite3_prepare_v2(db,
        "SELECT device_id, COUNT(*) as cnt, "
        "AVG(temperature) as avg_temp, MIN(temperature) as min_temp, "
        "MAX(temperature) as max_temp "
        "FROM sensor_data GROUP BY device_id;", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  设备=%d  记录数=%d  平均=%.1f°C  最低=%.1f°C  最高=%.1f°C\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3),
            sqlite3_column_double(stmt, 4));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 5. 使用回调方式查询（sqlite3_exec 的另一种用法）
    // ==========================================
    printf("\n=== 5. 使用回调方式查询 ===\n");
    auto callback = [](void *data, int argc, char **argv, char **col_names) -> int {
        for (int i = 0; i < argc; i++) {
            printf("  %s = %s", col_names[i], argv[i] ? argv[i] : "NULL");
        }
        printf("\n");
        return 0;  // 返回 0 继续，非 0 中断查询
    };
    char *err_msg = nullptr;
    sqlite3_exec(db, "SELECT * FROM devices WHERE type = 'sensor';",
                 callback, nullptr, &err_msg);
    if (err_msg) { sqlite3_free(err_msg); }

    sqlite3_close(db);
    remove("query_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
```

### 读取列值的函数

| 函数 | 返回类型 |
|------|----------|
| `sqlite3_column_int(stmt, col)` | `int` |
| `sqlite3_column_int64(stmt, col)` | `int64_t` |
| `sqlite3_column_double(stmt, col)` | `double` |
| `sqlite3_column_text(stmt, col)` | `const unsigned char*` |
| `sqlite3_column_blob(stmt, col)` | `const void*` |
| `sqlite3_column_bytes(stmt, col)` | `int` (数据长度) |
| `sqlite3_column_type(stmt, col)` | `int` (类型码) |

---

## 3.4 更新和删除数据（UPDATE / DELETE）

### SQL 语法

```sql
-- 更新
UPDATE 表名 SET 列1=新值1, 列2=新值2 WHERE 条件;

-- 删除
DELETE FROM 表名 WHERE 条件;

-- ⚠️ 不加 WHERE 会影响所有行！
UPDATE devices SET type='unknown';     -- 所有设备 type 被改成 unknown
DELETE FROM sensor_data;                -- 删除所有数据！
```

### 示例代码

文件：`examples/ch03_update_delete.cpp`

```cpp
#include <sqlite3.h>
#include <cstdio>

static bool exec_sql(sqlite3 *db, const char *sql) {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", err_msg);
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

static void print_devices(sqlite3 *db, const char *title) {
    printf("\n--- %s ---\n", title);
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db,
        "SELECT device_id, name, type, ip_address FROM devices;",
        -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  ID=%d  %-14s  %-10s  %s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2),
            sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "N/A");
    }
    sqlite3_finalize(stmt);
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("update_delete_demo.db", &db);

    // 准备测试数据
    exec_sql(db,
        "CREATE TABLE devices ("
        "  device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL, type TEXT NOT NULL, ip_address TEXT);");
    exec_sql(db,
        "INSERT INTO devices (name, type, ip_address) VALUES "
        "('温度传感器A', 'sensor', '192.168.1.101'),"
        "('温度传感器B', 'sensor', '192.168.1.102'),"
        "('网关设备', 'gateway', '192.168.1.1'),"
        "('旧设备', 'sensor', '192.168.1.200');");

    print_devices(db, "初始数据");

    // ==========================================
    // 1. UPDATE：修改特定设备的 IP 地址
    // ==========================================
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db,
        "UPDATE devices SET ip_address = ? WHERE device_id = ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, "10.0.0.101", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, 1);
    sqlite3_step(stmt);
    printf("\nUPDATE: 影响了 %d 行\n", sqlite3_changes(db));
    sqlite3_finalize(stmt);
    print_devices(db, "更新设备1的IP后");

    // ==========================================
    // 2. UPDATE：批量修改 - 将所有 sensor 的 type 改为 temp_sensor
    // ==========================================
    exec_sql(db, "UPDATE devices SET type = 'temp_sensor' WHERE type = 'sensor';");
    printf("UPDATE: 影响了 %d 行\n", sqlite3_changes(db));
    print_devices(db, "批量更新 type 后");

    // ==========================================
    // 3. DELETE：删除特定设备
    // ==========================================
    sqlite3_prepare_v2(db,
        "DELETE FROM devices WHERE name = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, "旧设备", -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    printf("\nDELETE: 影响了 %d 行\n", sqlite3_changes(db));
    sqlite3_finalize(stmt);
    print_devices(db, "删除 '旧设备' 后");

    // ==========================================
    // 4. sqlite3_changes() 和 sqlite3_total_changes()
    // ==========================================
    printf("\n本次连接总共修改了 %d 行\n", sqlite3_total_changes(db));

    sqlite3_close(db);
    remove("update_delete_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
```

---

## 3.5 本章小结

| 操作 | SQL | C API 执行方式 |
|------|-----|---------------|
| 创建表 | `CREATE TABLE` | `sqlite3_exec()` |
| 插入 | `INSERT INTO` | `sqlite3_prepare_v2()` + `sqlite3_bind_*()` + `sqlite3_step()` |
| 查询 | `SELECT` | `sqlite3_prepare_v2()` + `sqlite3_step()` + `sqlite3_column_*()` |
| 更新 | `UPDATE SET WHERE` | `sqlite3_prepare_v2()` + `sqlite3_bind_*()` + `sqlite3_step()` |
| 删除 | `DELETE FROM WHERE` | 同上 |

**关键原则：**
- 涉及外部输入的 SQL，**必须使用参数绑定**，不要拼接字符串
- 使用 `sqlite3_reset()` 可以复用已编译的语句
- `UPDATE` 和 `DELETE` **务必带 WHERE 子句**，否则影响所有行

---

[⬅ 第二章：SQLite 简介](ch02_sqlite_intro.md) | [目录](../README.md) | [第四章：高级查询 ➡](ch04_advanced_query.md)
