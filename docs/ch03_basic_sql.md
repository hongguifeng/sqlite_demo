# 第三章：SQL 基本操作（CRUD）

本章是 SQL 学习的核心，将详细讲解创建表、插入、查询、更新和删除操作。每个操作都有对应的 C++ 示例代码。

## 目录

1. [3.1 创建表（CREATE TABLE）](#31-创建表create-table)
2. [3.2 插入数据（INSERT）](#32-插入数据insert)
3. [3.3 查询数据（SELECT）](#33-查询数据select)
4. [3.4 更新和删除数据（UPDATE / DELETE）](#34-更新和删除数据update--delete)
5. [3.5 本章小结](#35-本章小结)

### 本章重点 API 导航

1. `sqlite3_exec()`：适合执行一次性、无结果集的 SQL
2. `sqlite3_prepare_v2()`：把 SQL 编译成可复用的语句对象
3. `sqlite3_bind_*()`：给 SQL 占位符绑定参数
4. `sqlite3_step()`：执行语句或获取下一行结果
5. `sqlite3_column_*()`：读取当前结果行中的列值
6. `sqlite3_reset()` / `sqlite3_clear_bindings()`：复用语句对象
7. `sqlite3_finalize()`：释放语句对象

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

传统数据库（如 MySQL、PostgreSQL）使用**严格类型系统**：每列在建表时必须声明精确类型（如 `INT`、`VARCHAR(255)`、`FLOAT`），数据库会强制检查并拒绝不匹配的值，类型一旦确定不可隐式改变。

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

### 深入理解：什么是外键（Foreign Key）

前面的约束大多是在限制“本表这一列自身”的取值，例如：

1. 主键要求唯一。
2. `NOT NULL` 要求不能留空。
3. `UNIQUE` 要求不能重复。

而**外键**约束处理的是另一类问题：

> 当前表中的某个值，必须能在另一张表里找到对应记录。

在本章示例里：

```sql
CREATE TABLE devices (
    device_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY,
    device_id INTEGER NOT NULL,
    temperature REAL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);
```

这里的关系可以这样理解：

1. `devices` 是**父表**，保存设备主记录。
2. `sensor_data` 是**子表**，保存某台设备产生的采样数据。
3. `sensor_data.device_id` 是外键，引用 `devices.device_id`。

它表达的业务含义是：

> 每一条传感器数据，都应该属于一台真实存在的设备。

如果没有外键，就可能出现这种“脏数据”：

```sql
INSERT INTO sensor_data (device_id, temperature) VALUES (999, 25.3);
```

假设 `devices` 表里根本没有 `device_id = 999` 这台设备，那么这条采样数据就变成了“找不到归属设备的孤儿记录”。

外键的作用，就是帮助数据库在结构层面阻止这种不一致。

### 用嵌入式思维理解外键

如果把数据库表想成 C 里的两组结构体数组：

```c
struct Device {
    int device_id;
    char name[64];
};

struct SensorData {
    int id;
    int device_id;   // 应该引用某个真实存在的 Device.device_id
    double temperature;
};
```

那么外键约束，本质上相当于你在要求：

1. `SensorData.device_id` 不能随便填一个整数；
2. 它必须对应 `devices[]` 数组里某个已经存在的设备 ID；
3. 删除设备时，还要考虑有没有 `sensor_data[]` 在引用它。

也就是说，外键保护的不是“单个字段格式”，而是**两张表之间的引用关系**。

### 外键到底能帮你防住什么问题

把外键打开后，数据库通常会帮你检查两类典型错误。

#### 1. 插入或更新子表时，引用了不存在的父表记录

例如：

```sql
INSERT INTO sensor_data (device_id, temperature) VALUES (999, 25.3);
```

如果 `devices` 里不存在 `device_id = 999`，那么这条插入应当失败。

同理，下面这种更新也可能失败：

```sql
UPDATE sensor_data SET device_id = 999 WHERE id = 1;
```

因为它把原本合法的引用，改成了一个不存在的设备。

#### 2. 删除或修改父表时，破坏了子表的引用关系

例如：

```sql
DELETE FROM devices WHERE device_id = 1;
```

如果 `sensor_data` 里还有很多记录的 `device_id = 1`，那么数据库需要决定怎么处理这些子表记录。常见策略包括：

1. 直接拒绝删除。
2. 连同相关传感器数据一起删除。
3. 把子表里的外键列置为 `NULL`。

这部分行为由 `ON DELETE` / `ON UPDATE` 子句决定，后面会继续解释。

### 外键语法怎么读

最基本的写法是：

```sql
FOREIGN KEY (子表列) REFERENCES 父表(被引用列)
```

套到本章示例就是：

```sql
FOREIGN KEY (device_id) REFERENCES devices(device_id)
```

可以按下面的顺序去理解：

1. 子表当前这一列是 `device_id`。
2. 它引用的是 `devices` 表。
3. 引用目标列是 `devices.device_id`。

这通常要求被引用列是主键，或者至少具有唯一性。

### SQLite 中一个非常重要的细节：外键默认可能并不会自动生效

这是很多初学者最容易踩坑的地方。

在 SQLite 中，**写了 `FOREIGN KEY (...) REFERENCES ...` 并不等于当前连接一定会执行外键检查**。默认情况下，外键约束是否生效，取决于连接是否开启了外键支持。

最常见的开启方式是：

```cpp
sqlite3 *db = nullptr;
sqlite3_open("demo.db", &db);
sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
```

也可以查询当前连接是否已经开启：

```sql
PRAGMA foreign_keys;
```

返回值通常为：

1. `1`：已开启。
2. `0`：未开启。

这里要特别记住两个要点：

1. **这是按连接生效的设置**，不是“给数据库文件开一次就永久开启”。
2. 你通常应该在 `sqlite3_open()` 之后、执行建表和增删改查之前立即打开它。

所以，本章里虽然在表结构中声明了外键，但前面的几个 CRUD 示例主要是讲 API 用法，并没有专门演示外键报错场景。如果你希望自己的程序真正依赖外键约束，就要在连接建立后显式执行：

```cpp
exec_sql(db, "PRAGMA foreign_keys = ON;", "开启外键检查");
```

### 一个最小示例：打开外键后会发生什么

```sql
CREATE TABLE devices (
    device_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY,
    device_id INTEGER NOT NULL,
    temperature REAL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id)
);

INSERT INTO devices (device_id, name) VALUES (1, '温度传感器A');

-- 合法：父表里存在 device_id = 1
INSERT INTO sensor_data (device_id, temperature) VALUES (1, 25.3);

-- 非法：父表里不存在 device_id = 999
INSERT INTO sensor_data (device_id, temperature) VALUES (999, 18.0);
```

如果外键检查已开启，最后一条语句通常会失败，并返回类似“foreign key constraint failed”的错误。

这正是外键的价值：

> 它把“数据关系是否一致”的检查，从应用代码里的一堆手工判断，前移到了数据库约束层。

### `ON DELETE` / `ON UPDATE` 是什么

只声明 `REFERENCES` 还不够，很多时候你还需要定义：当父表记录被删除或主键被修改时，子表应该怎么办。

例如：

```sql
FOREIGN KEY (device_id)
    REFERENCES devices(device_id)
    ON DELETE CASCADE
    ON UPDATE CASCADE
```

常见动作如下：

| 子句 | 含义 | 典型场景 |
|------|------|---------|
| `NO ACTION` / `RESTRICT` | 拒绝本次删除或更新 | 希望强制先清理子表数据 |
| `CASCADE` | 父表变更同步传播到子表 | 父子记录生命周期强绑定 |
| `SET NULL` | 把子表外键列置空 | 子记录允许“暂时失去归属” |
| `SET DEFAULT` | 子表外键列改为默认值 | 设计了默认归属目标 |

对本章的设备-采样数据模型来说，最常见的两种设计是：

1. `ON DELETE RESTRICT`：设备还有采样数据时，不允许删设备。
2. `ON DELETE CASCADE`：删设备时，相关采样数据也一起删掉。

选哪一种，不是 SQL 语法问题，而是业务规则问题。

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

    exec_sql(db, "PRAGMA foreign_keys = ON;", "开启外键检查");

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

    // 5. 查看当前连接是否已开启外键检查
    printf("\n--- foreign_keys 开关状态 ---\n");
    rc = sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  foreign_keys = %d\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // 6. 查看 sensor_data 表的外键定义
    printf("\n--- sensor_data 的外键定义 ---\n");
    rc = sqlite3_prepare_v2(db,
        "PRAGMA foreign_key_list(sensor_data);", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  外键: sensor_data.%s -> %s.%s\n",
                sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 4));
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
3. **`sqlite_master`**：SQLite 内部的系统表，记录了数据库中所有的表、索引、视图、触发器等对象定义
4. **`PRAGMA table_info()`**：查看表结构的特殊命令，可返回每一列的元数据
5. **`PRAGMA foreign_keys`**：查看当前连接是否开启了外键检查
6. **`PRAGMA foreign_key_list(表名)`**：查看某张表声明了哪些外键关系
7. **外键声明只定义关系，不代表当前连接一定开启了外键检查**；在真实项目中应尽早执行 `PRAGMA foreign_keys = ON;`

### 深入理解：`sqlite_master`

在很多数据库里，系统都会维护一套“元数据表”，用来描述数据库自身的结构。SQLite 中最常见的就是 `sqlite_master`。

它可以理解为“数据库对象目录表”，里面保存了当前数据库中的对象定义，例如：

| 字段 | 含义 |
|------|------|
| `type` | 对象类型，如 `table`、`index`、`view`、`trigger` |
| `name` | 对象名称 |
| `tbl_name` | 该对象所属的表名 |
| `rootpage` | 对应 B-Tree 根页页号 |
| `sql` | 创建该对象时的原始 SQL |

示例中的查询语句如下：

```sql
SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;
```

这条语句可以拆开理解：

1. **`SELECT name`**：只取对象名称，不关心其他元数据列。
2. **`FROM sqlite_master`**：从 SQLite 的系统元数据表中读取信息。
3. **`WHERE type='table'`**：只筛选类型为 `table` 的对象，也就是数据库中的表。
4. **`ORDER BY name`**：按名称升序排序，便于稳定输出和人工查看。

执行后，通常会看到我们创建的 `devices` 和 `sensor_data`。如果表使用了 `AUTOINCREMENT`，还可能看到 SQLite 自动维护的内部表 `sqlite_sequence`。

> `sqlite_sequence` 用来记录带 `AUTOINCREMENT` 的表当前已经分配到的自增值，因此它也会出现在 `type='table'` 的查询结果里。

如果你想进一步查看某张表的建表 SQL，可以这样写：

```sql
SELECT name, sql FROM sqlite_master WHERE type='table';
```

这在调试“表到底是按什么结构创建出来的”时很有帮助。

### 深入理解：`PRAGMA table_info()`

`PRAGMA` 是 SQLite 提供的一类特殊命令，不完全等同于普通 SQL 的增删改查语句。它主要用于：

1. 查询数据库内部信息。
2. 获取表、索引等对象的元数据。
3. 读取或设置某些运行参数。

示例中使用的是：

```sql
PRAGMA table_info(devices);
```

它的作用是返回 `devices` 表中每一列的定义信息。返回结果中，每一行代表一列，常见字段如下：

| 返回列 | 含义 |
|--------|------|
| `cid` | 列编号，从 0 开始 |
| `name` | 列名 |
| `type` | 声明的数据类型 |
| `notnull` | 是否声明为 `NOT NULL`，1 表示是 |
| `dflt_value` | 默认值 |
| `pk` | 是否属于主键，非 0 表示是 |

因此，代码中的输出：

```cpp
printf("  列: %-15s 类型: %-10s 非空: %d 默认: %s\n",
    sqlite3_column_text(stmt, 1),   // name
    sqlite3_column_text(stmt, 2),   // type
    sqlite3_column_int(stmt, 3),    // notnull
    sqlite3_column_text(stmt, 4) ?
        (const char*)sqlite3_column_text(stmt, 4) : "(无)");
```

本质上是在读取 `PRAGMA table_info(devices)` 返回结果中的第 1、2、3、4 列，也就是：

1. 列名 `name`
2. 数据类型 `type`
3. 是否非空 `notnull`
4. 默认值 `dflt_value`

这样做的意义是：程序创建完表之后，可以立即读取数据库实际记录下来的表结构，验证：

1. 列是否真的创建成功。
2. 数据类型是否符合预期。
3. `NOT NULL` 和 `DEFAULT` 等约束是否已经生效。

这是一种很适合教学和调试的做法，因为它不是“相信自己写对了 SQL”，而是“直接向 SQLite 查询实际结果”。

### 补充：为什么这里要用 `sqlite3_prepare_v2()`

虽然前面创建表时使用的是 `sqlite3_exec()`，但查看表列表和表结构时改用了 `sqlite3_prepare_v2()`，原因是这两条语句会返回结果集。

两者的典型分工可以这样理解：

| 场景 | 更适合的 API |
|------|-------------|
| 不关心返回行，只想执行 SQL | `sqlite3_exec()` |
| 需要逐行读取查询结果 | `sqlite3_prepare_v2()` + `sqlite3_step()` + `sqlite3_column_*()` |

所以：

1. `CREATE TABLE` 这种 DDL 语句，用 `sqlite3_exec()` 最直接。
2. `SELECT ...` 和 `PRAGMA table_info(...)` 这类会返回多行结果的语句，更适合使用预编译语句接口逐行读取。

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

    exec_sql(db, "PRAGMA foreign_keys = ON;");

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

    // 演示外键约束：插入一条引用不存在设备的采样数据
    printf("\n--- 外键约束演示 ---\n");
    sqlite3_prepare_v2(db, sql_sensor, -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, 999);
    sqlite3_bind_double(stmt, 2, 18.0);
    sqlite3_bind_double(stmt, 3, 50.0);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("插入不存在设备的采样数据失败: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

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

这个示例现在会额外演示一件很关键的事：当 `PRAGMA foreign_keys = ON;` 已开启时，往 `sensor_data` 里插入 `device_id = 999` 这种“父表中不存在的设备”会失败，并返回外键约束错误。这样读者不只看到外键的定义，还能直接看到它在运行时如何拦住脏数据。

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

### 深入理解：为什么推荐“预编译语句 + 参数绑定”

在示例代码里，插入数据用了两种方式：

1. 直接把完整 SQL 字符串交给 `sqlite3_exec()` 执行。
2. 先 `sqlite3_prepare_v2()`，再 `sqlite3_bind_*()` 绑定参数，最后 `sqlite3_step()` 执行。

虽然两种方式都能把数据插进去，但在工程实践里，第二种几乎总是更推荐，原因主要有三点。

#### 1. 安全：避免 SQL 注入

如果把外部输入直接拼接进 SQL 字符串，用户输入就可能“逃逸”出原本的数据位置，变成 SQL 语句的一部分。

例如：

```cpp
char sql[256];
sprintf(sql, "INSERT INTO devices (name) VALUES ('%s');", user_input);
```

如果 `user_input` 里恰好包含引号、分号甚至恶意 SQL，那么数据库执行的就不再是你期望的插入语句。

而参数绑定的做法是：

```cpp
sqlite3_prepare_v2(db,
    "INSERT INTO devices (name, type, ip_address) VALUES (?, ?, ?);",
    -1, &stmt, nullptr);
```

这里的 `?` 是参数占位符。后面通过 `sqlite3_bind_text()`、`sqlite3_bind_int()` 等函数把数据值绑定进去。SQLite 会把这些值当成“数据”处理，而不是当成 SQL 语法的一部分去解析，因此可以有效防止 SQL 注入。

#### 2. 性能：SQL 只编译一次，可以重复执行

SQLite 执行一条 SQL，大致要经历两个阶段：

1. 解析 SQL 文本并编译成内部指令；
2. 执行这些指令。

如果你每插入一条数据都重新拼接一次 SQL，再交给 `sqlite3_exec()`，SQLite 就需要反复做“解析 + 编译”。

而预编译语句的模式是：

1. 先调用 `sqlite3_prepare_v2()` 编译一次 SQL 模板；
2. 后续只替换参数值；
3. 多次调用 `sqlite3_step()` 重复执行。

这样在批量插入场景下会明显更高效，也更符合数据库编程的常规写法。

#### 3. 可维护性：代码结构更清晰

把 SQL 模板和参数值分开后，代码会更容易阅读：

1. SQL 本身描述“要做什么”；
2. `bind` 调用描述“这次传入什么值”；
3. `step` 表示“现在真正执行”。

这种写法非常适合后续扩展，例如改成循环插入、统一错误处理、封装数据库访问层等。

### 深入理解：插入一条记录时，SQLite C API 实际经历了什么

下面这段代码：

```cpp
const char *sql = "INSERT INTO devices (name, type, ip_address) VALUES (?, ?, ?);";
int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

sqlite3_bind_text(stmt, 1, "温度传感器B", -1, SQLITE_TRANSIENT);
sqlite3_bind_text(stmt, 2, "sensor", -1, SQLITE_TRANSIENT);
sqlite3_bind_text(stmt, 3, "192.168.1.102", -1, SQLITE_TRANSIENT);

rc = sqlite3_step(stmt);
```

从执行流程看，可以分成 4 步：

1. **准备语句**：`sqlite3_prepare_v2()` 把 SQL 文本编译成一个可执行的语句对象 `stmt`。
2. **绑定参数**：`sqlite3_bind_*()` 把第 1、2、3 个占位符分别替换成实际值。
3. **执行语句**：`sqlite3_step(stmt)` 让 SQLite 真正执行这次插入。
4. **检查结果**：对于 `INSERT`、`UPDATE`、`DELETE` 这类不返回结果行的语句，执行成功时通常返回 `SQLITE_DONE`。

这里有一个很重要的概念：

> `stmt` 不是“SQL 字符串”，而是“已经编译好的 SQL 执行对象”。

所以你可以把它理解成“已经装填好的命令模板”，只需要给模板填参数，就能多次执行。

### `sqlite3_step()` 的返回值怎么理解

`sqlite3_step()` 是 SQLite C API 中最核心的函数之一，不同类型的 SQL，返回值含义略有不同：

| 返回值 | 含义 |
|--------|------|
| `SQLITE_ROW` | 成功取到一行结果，常见于 `SELECT` |
| `SQLITE_DONE` | 执行完成，没有更多结果行，常见于 `INSERT/UPDATE/DELETE` |
| 其他错误码 | 执行失败，例如约束冲突、语法错误、数据库忙等 |

因此在插入示例中：

```cpp
if (rc == SQLITE_DONE) {
    printf("插入设备 2，rowid = %lld\n", sqlite3_last_insert_rowid(db));
}
```

这里的判断逻辑就是：只有在确认 `sqlite3_step()` 已经执行完插入后，才去读取这次插入得到的 `rowid`。

### 深入理解：参数下标为什么从 1 开始

很多 C/C++ 程序员第一次接触 SQLite 时，都会注意到这一点：

```cpp
sqlite3_bind_text(stmt, 1, ...);
sqlite3_bind_text(stmt, 2, ...);
sqlite3_bind_text(stmt, 3, ...);
```

这里的参数索引不是从 0 开始，而是从 1 开始。

原因是 SQLite 的占位符编号规则如此规定：

1. 第一个 `?` 对应索引 1
2. 第二个 `?` 对应索引 2
3. 第三个 `?` 对应索引 3

如果 SQL 中使用的是命名参数，例如 `:name`、`@name`、`$name`，也可以通过对应接口按名字查找或绑定。

### 深入理解：为什么插入完一条后要 `reset` 和 `clear_bindings`

在示例中，插入第 2 个设备后，代码执行了：

```cpp
sqlite3_reset(stmt);
sqlite3_clear_bindings(stmt);
```

这两个函数经常一起出现，但职责并不完全相同。

#### `sqlite3_reset(stmt)`

作用是把语句对象恢复到“可再次执行”的状态。

你可以把它理解为：

1. 上一次 `sqlite3_step()` 已经跑完了；
2. 现在要把执行游标和内部状态复位；
3. 这样下一次才能重新执行同一个 `stmt`。

如果不调用 `sqlite3_reset()`，通常不能直接拿这个语句继续执行下一次。

#### `sqlite3_clear_bindings(stmt)`

作用是把当前已经绑定在占位符上的值清空。

这一步不是每次都绝对必须，但非常推荐，原因是它能避免“上一轮参数残留”造成的隐蔽错误。

例如：

1. 上一轮绑定了 3 个参数；
2. 下一轮你不小心漏绑了其中一个；
3. 如果不清空，旧值可能还留在那个参数位置上；
4. 结果 SQL 仍然能执行，但插入的是混杂了旧参数的数据。

所以，教程里把 `reset + clear_bindings` 连起来写，是一种很适合初学者掌握的安全写法。

### 深入理解：`sqlite3_last_insert_rowid()` 返回的是什么

示例中每插入一条设备后，都打印了：

```cpp
sqlite3_last_insert_rowid(db)
```

它返回的是：

> 当前数据库连接 `db` 最近一次成功 `INSERT` 后生成的行 ID。

需要注意三点：

1. 这是“当前连接”的最近插入结果，不是全局数据库范围的最近插入。
2. 它通常对应 `INTEGER PRIMARY KEY` 的值。
3. 如果表不是这种主键设计，或者插入方式特殊，返回值的含义要结合表结构理解。

在本例里，`devices.device_id` 是 `INTEGER PRIMARY KEY AUTOINCREMENT`，所以这个值可以直观理解为新插入设备的主键 ID。

### 深入理解：批量插入时为什么循环复用同一个 `stmt`

批量插入传感器数据时，示例代码只 `prepare` 了一次：

```cpp
const char *sql_sensor =
    "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);";
sqlite3_prepare_v2(db, sql_sensor, -1, &stmt, nullptr);
```

然后在循环里反复做：

1. `sqlite3_bind_int()` / `sqlite3_bind_double()` 绑定当前这一条数据；
2. `sqlite3_step(stmt)` 执行插入；
3. `sqlite3_reset(stmt)` 复位语句；
4. `sqlite3_clear_bindings(stmt)` 清空旧参数。

这种写法的好处是：

1. SQL 只编译一次，效率更高；
2. 循环结构清晰，适合处理数组、传感器缓冲区、采集队列等批量数据；
3. 后续很容易再套一层事务，进一步提升性能。

对于嵌入式或边缘设备场景，这种模式非常常见，因为传感器数据往往天然就是“多条连续写入”。

### 补充：字符串绑定里的 `SQLITE_TRANSIENT` 和 `SQLITE_STATIC`

这一点虽然前面提过，但值得再展开一下，因为它是 C 接口里一个很典型的生命周期问题。

以这句为例：

```cpp
sqlite3_bind_text(stmt, 1, "温度传感器B", -1, SQLITE_TRANSIENT);
```

最后一个参数告诉 SQLite：你如何处理这块字符串内存。

#### `SQLITE_TRANSIENT`

含义是：

1. SQLite 立刻自己复制一份字符串内容；
2. 原始字符串之后是否变化、是否被释放，都不会影响 SQLite。

这也是最安全、最省心的选择，尤其适合：

1. 临时缓冲区；
2. 函数局部变量；
3. 来自外部输入、生命周期不易保证的字符串。

#### `SQLITE_STATIC`

含义是：

1. SQLite 不复制字符串；
2. 它假设这块内存在 SQLite 使用期间一直有效。

这适合绑定静态字符串常量，或者你能严格保证生命周期的内存。但如果判断失误，就可能出现悬空指针问题。

因此，对初学者或教程代码来说，优先使用 `SQLITE_TRANSIENT` 是合理的。

### 小结：插入操作的推荐心智模型

可以把一条带参数的 `INSERT` 理解成下面这个固定流程：

1. 写出带 `?` 占位符的 SQL 模板。
2. 用 `sqlite3_prepare_v2()` 编译成 `stmt`。
3. 用 `sqlite3_bind_*()` 绑定这次要插入的值。
4. 用 `sqlite3_step()` 真正执行。
5. 若要复用，调用 `sqlite3_reset()` 和 `sqlite3_clear_bindings()`。
6. 全部完成后，调用 `sqlite3_finalize()` 释放语句对象。

只要掌握了这个流程，后面的 `UPDATE`、`DELETE`、带条件的 `SELECT`，基本都只是“SQL 文本不同”，API 使用模式是同一套。

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

### 专题：`sqlite3_step()` 使用教程

`sqlite3_step()` 是 SQLite C API 里最核心的执行函数。几乎所有通过 `sqlite3_prepare_v2()` 得到的语句对象，最后都要靠它来真正运行。

可以把它先粗略理解成：

> “让这条已经编译好的 SQL 往前走一步。”

但这“一步”在不同 SQL 场景中的含义并不完全一样。

#### 场景 1：执行 `INSERT / UPDATE / DELETE`

这类语句通常不返回结果集，调用一次 `sqlite3_step()` 就够了：

```cpp
sqlite3_prepare_v2(db,
    "UPDATE devices SET ip_address = ? WHERE device_id = ?;",
    -1, &stmt, nullptr);
sqlite3_bind_text(stmt, 1, "10.0.0.101", -1, SQLITE_STATIC);
sqlite3_bind_int(stmt, 2, 1);

int rc = sqlite3_step(stmt);
if (rc == SQLITE_DONE) {
    printf("更新成功\n");
}
```

这里的理解方式是：

1. `prepare` 只是准备好语句；
2. `bind` 只是把参数填进去；
3. 真正让数据库执行修改动作的是 `sqlite3_step()`。

执行成功后，最常见的返回值是 `SQLITE_DONE`。

#### 场景 2：执行 `SELECT`

查询语句会返回多行结果，因此 `sqlite3_step()` 往往要反复调用：

```cpp
sqlite3_prepare_v2(db,
    "SELECT device_id, name FROM devices;",
    -1, &stmt, nullptr);

while (sqlite3_step(stmt) == SQLITE_ROW) {
    printf("ID=%d 名称=%s\n",
        sqlite3_column_int(stmt, 0),
        sqlite3_column_text(stmt, 1));
}
```

在查询场景下，可以把它理解成：

1. 第一次 `step`：尝试取第 1 行；
2. 第二次 `step`：尝试取第 2 行；
3. 不断重复，直到没有更多行；
4. 读完时返回 `SQLITE_DONE`。

所以在 `SELECT` 里，`sqlite3_step()` 更像“取下一行”的动作。

#### `sqlite3_step()` 最常见的返回值

| 返回值 | 典型含义 | 常见场景 |
|--------|----------|----------|
| `SQLITE_ROW` | 成功取到一行结果 | `SELECT`、`PRAGMA`、元数据查询 |
| `SQLITE_DONE` | 执行完成，没有更多结果 | `INSERT`、`UPDATE`、`DELETE`，或查询读完 |
| 其他错误码 | 执行失败 | 约束冲突、SQL 错误、数据库忙等 |

#### 一段最小可复用心智模型

学习 `sqlite3_step()` 时，建议把它记成下面这两句：

1. 对修改类语句，`step` 表示“执行这条语句”。
2. 对查询类语句，`step` 表示“取下一行结果”。

只要这两句分清楚，后面的 SQLite C API 代码就会容易读很多。

#### `sqlite3_step()` 常见使用错误

1. 以为 `prepare` 后 SQL 已经执行，实际上没有，真正执行是在 `step`。
2. 查询场景下只调一次 `step`，结果只读到第一行。
3. 执行后不检查返回值，导致失败时没有及时发现。
4. 复用语句对象前不 `sqlite3_reset()`，导致下一次执行状态不正确。

### 专题：`sqlite3_column_*()` 使用教程

`sqlite3_column_*()` 这一组函数专门用于读取“当前结果行”中的列值。它们只能在 `sqlite3_step(stmt) == SQLITE_ROW` 时使用，离开当前行后再读就没有意义了。

可以把它们理解成：

> “从当前这一行的第 N 列里，把值按某种类型取出来。”

#### 最基本的使用前提

只有当这句成立时：

```cpp
sqlite3_step(stmt) == SQLITE_ROW
```

你才有一行有效结果可读。然后再调用：

```cpp
sqlite3_column_int(stmt, 0)
sqlite3_column_text(stmt, 1)
sqlite3_column_double(stmt, 2)
```

#### 如何确定“第几列”

结果列索引从 **0** 开始，并且顺序由 SQL 的 `SELECT` 列表决定。

例如：

```sql
SELECT device_id, name, temperature FROM sensor_data_view;
```

那么读取时就是：

1. 第 0 列：`device_id`
2. 第 1 列：`name`
3. 第 2 列：`temperature`

这里要特别注意：这和表定义顺序不一定完全相同，因为你完全可以在查询里调换列顺序、使用表达式、别名、聚合函数。

#### 最常用的几种读取函数

| 函数 | 适合读取 |
|------|----------|
| `sqlite3_column_int()` | 整数值 |
| `sqlite3_column_int64()` | 64 位整数 |
| `sqlite3_column_double()` | 浮点数 |
| `sqlite3_column_text()` | 文本字符串 |
| `sqlite3_column_blob()` | 二进制数据 |
| `sqlite3_column_type()` | 当前值的实际类型 |
| `sqlite3_column_bytes()` | 文本或二进制长度 |

#### 一个典型例子

```cpp
sqlite3_prepare_v2(db,
    "SELECT device_id, name, ip_address FROM devices;",
    -1, &stmt, nullptr);

while (sqlite3_step(stmt) == SQLITE_ROW) {
    int device_id = sqlite3_column_int(stmt, 0);
    const unsigned char *name = sqlite3_column_text(stmt, 1);
    const unsigned char *ip = sqlite3_column_text(stmt, 2);

    printf("ID=%d 名称=%s IP=%s\n",
        device_id,
        name ? (const char*)name : "(空)",
        ip ? (const char*)ip : "(空)");
}
```

这段代码体现了三个关键点：

1. 整数列用 `sqlite3_column_int()` 读取；
2. 文本列用 `sqlite3_column_text()` 读取；
3. 可能为 `NULL` 的文本要先判空。

#### 为什么 `sqlite3_column_text()` 经常要判空

因为数据库里的某一列可能是 `NULL`。如果当前列值是 `NULL`，`sqlite3_column_text()` 可能返回空指针。

因此像下面这种写法很常见：

```cpp
const unsigned char *text = sqlite3_column_text(stmt, 2);
printf("%s\n", text ? (const char*)text : "(无)");
```

更稳妥的方式还可以先检查：

```cpp
if (sqlite3_column_type(stmt, 2) == SQLITE_NULL) {
    printf("(无)\n");
}
```

#### `sqlite3_column_type()` 什么时候特别有用

当你不确定某一列当前值是不是 `NULL`，或者查询结果里可能出现多种类型时，`sqlite3_column_type()` 很有帮助。

常见场景：

1. 列允许为空；
2. 查询里用了表达式或函数；
3. 你在写通用查询打印工具，而不是针对固定列结构的业务代码。

#### `sqlite3_column_*()` 常见使用错误

1. 把结果列索引当成从 1 开始。
2. 按表结构顺序读，而不是按 `SELECT` 输出列顺序读。
3. 不处理 `NULL`，直接把文本指针拿去打印。
4. 在 `sqlite3_step()` 还没返回 `SQLITE_ROW` 时就读列值。
5. 读取类型和结果语义不匹配，例如把浮点统计值用 `sqlite3_column_int()` 强行读取。

#### 一段最小实战口诀

学习 `sqlite3_column_*()` 时，可以记成：

1. 先 `step` 到 `SQLITE_ROW`；
2. 再按 `SELECT` 列顺序读取；
3. 整数、浮点、文本分别用对应函数；
4. 可能为 `NULL` 的列先判空或先看 `sqlite3_column_type()`。

### 深入理解：查询语句的标准执行流程

和插入、更新不同，`SELECT` 的核心区别在于它通常会返回多行结果。因此查询操作的典型流程是：

1. 用 `sqlite3_prepare_v2()` 预编译 SQL。
2. 如果有条件参数，先用 `sqlite3_bind_*()` 绑定。
3. 反复调用 `sqlite3_step()`，一行一行取结果。
4. 每拿到一行后，用 `sqlite3_column_*()` 读取各列值。
5. 读完后调用 `sqlite3_finalize()` 释放语句对象。

可以把它理解成下面的模式：

```cpp
sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

while (sqlite3_step(stmt) == SQLITE_ROW) {
    // 读取当前这一行的各列数据
}

sqlite3_finalize(stmt);
```

这个模式几乎适用于所有返回结果集的查询，包括：

1. 普通 `SELECT`
2. 带 `WHERE` 的条件查询
3. 带 `ORDER BY`、`LIMIT` 的排序分页查询
4. 聚合查询
5. 很多会返回结果集的 `PRAGMA` 查询

### 深入理解：为什么查询时要反复调用 `sqlite3_step()`

在插入场景中，`sqlite3_step()` 执行一次通常就结束了；但在查询场景中，它更像是“从结果集游标中取下一行”。

典型行为如下：

1. 第一次调用 `sqlite3_step(stmt)`，SQLite 尝试取第 1 行结果。
2. 如果成功取到，返回 `SQLITE_ROW`。
3. 再调用一次，就继续取第 2 行。
4. 如此循环，直到没有更多数据。
5. 当所有结果都取完时，返回 `SQLITE_DONE`。

所以：

```cpp
while (sqlite3_step(stmt) == SQLITE_ROW) {
    ...
}
```

这段代码的本质含义就是：

> 只要还能取到一行结果，就持续处理这一行。

这也是数据库游标式读取最常见的写法。

### 深入理解：`sqlite3_column_*()` 读的是“当前行”

当 `sqlite3_step()` 返回 `SQLITE_ROW` 时，说明当前游标已经停在某一行上。这时调用：

```cpp
sqlite3_column_int(stmt, 0)
sqlite3_column_text(stmt, 1)
sqlite3_column_double(stmt, 2)
```

读取到的都是“当前这一行”对应列的数据。

这里有两个关键点：

1. 列索引 `col` 从 **0 开始**。
2. 列索引顺序对应 SQL 里 `SELECT` 出来的列顺序，而不一定是表定义里的物理顺序。

例如：

```sql
SELECT device_id, name, type, ip_address FROM devices;
```

则：

1. 第 0 列是 `device_id`
2. 第 1 列是 `name`
3. 第 2 列是 `type`
4. 第 3 列是 `ip_address`

所以示例中的这段代码：

```cpp
printf("  ID=%d  名称=%-12s  类型=%-8s  IP=%s\n",
    sqlite3_column_int(stmt, 0),
    sqlite3_column_text(stmt, 1),
    sqlite3_column_text(stmt, 2),
    sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "N/A");
```

本质上就是在读取当前行的 4 个输出列。

### 参数下标和列下标的区别

这部分很容易混淆，尤其是刚接触 SQLite C API 时。

必须记住：

1. `sqlite3_bind_*()` 绑定参数时，下标从 **1 开始**。
2. `sqlite3_column_*()` 读取结果列时，下标从 **0 开始**。

也就是说：

```cpp
sqlite3_bind_double(stmt, 1, 25.0);   // 第 1 个占位符
sqlite3_column_double(stmt, 2);        // 第 3 个结果列
```

这是 SQLite API 里一个很常见、但必须牢记的细节。

### 深入理解：SQLite 查询结果的类型并不总是“严格等于建表类型”

SQLite 是动态类型系统，`sqlite3_column_*()` 读取数据时，并不是简单做“声明类型匹配”。

更准确地说：

1. SQL 表达式在当前结果行上会生成一个值；
2. 这个值在 SQLite 内部有当前运行时类型；
3. `sqlite3_column_*()` 会尝试按你请求的方式取出它。

例如：

1. 用 `sqlite3_column_int()` 读取一个数值型结果，通常没问题。
2. 用 `sqlite3_column_text()` 读取数值结果，SQLite 也可能返回其文本表示。
3. 如果是 `NULL`，就需要特别处理，否则容易输出空指针或得到非预期值。

因此，在写教程或工程代码时，最好遵循一个原则：

> 查询时尽量按“结果语义”选择合适的 `sqlite3_column_*()` 读取函数，而不是随便混用。

### 为什么这里要判断 `sqlite3_column_text(stmt, 3)` 是否为空

示例中有这样的写法：

```cpp
sqlite3_column_text(stmt, 3) ? (const char*)sqlite3_column_text(stmt, 3) : "N/A"
```

这是因为 `ip_address` 列允许为 `NULL`。如果当前记录没有 IP 地址，直接把空指针当作字符串打印，会导致未定义行为或输出错误。

所以这里的逻辑是：

1. 先检查第 3 列是否为非空；
2. 如果非空，就转成 `const char*` 输出；
3. 如果为空，就打印 `N/A`。

这是一种简单但实用的空值处理方式。

更稳妥的写法还可以先用：

```cpp
sqlite3_column_type(stmt, 3)
```

判断这一列当前是否为 `SQLITE_NULL`，再决定如何读取。

### 深入理解：条件查询的核心并不在 `WHERE`，而在“参数化 WHERE”

文档中的条件查询示例：

```cpp
sqlite3_prepare_v2(db,
    "SELECT id, device_id, temperature, humidity FROM sensor_data "
    "WHERE temperature > ?;", -1, &stmt, nullptr);
sqlite3_bind_double(stmt, 1, 25.0);
```

它表面上是在演示 `WHERE temperature > 25.0`，但更重要的教学点其实是：

> 查询条件也应该参数化，而不是把阈值直接拼接到 SQL 字符串里。

这样做的好处和插入操作一致：

1. 避免 SQL 注入；
2. SQL 模板可以复用；
3. 代码更清晰，便于动态传入不同查询条件。

对于实际项目来说，几乎所有带用户输入的查询条件，都应该写成这种“`WHERE ... ?` + bind”的形式。

### 深入理解：`ORDER BY` 和 `LIMIT` 在结果读取前就已经生效

示例中的排序查询：

```sql
SELECT device_id, temperature FROM sensor_data
ORDER BY temperature DESC LIMIT 3;
```

其含义是：

1. 先从 `sensor_data` 中选出指定列；
2. 按 `temperature` 从高到低排序；
3. 再只保留前 3 条结果。

因此，C++ 代码中的循环：

```cpp
while (sqlite3_step(stmt) == SQLITE_ROW) {
    ...
}
```

读到的已经是“排好序且截断后”的结果，不需要再在 C++ 里手工排序或裁剪。

这也是数据库查询的一个重要思想：

> 能在 SQL 层完成的数据筛选、排序、限制，尽量交给数据库做，而不是先全量取回再在应用层处理。

### 深入理解：聚合查询返回的已经不是“原始记录”，而是“统计结果”

示例中的聚合查询：

```sql
SELECT device_id, COUNT(*) as cnt,
AVG(temperature) as avg_temp, MIN(temperature) as min_temp,
MAX(temperature) as max_temp
FROM sensor_data GROUP BY device_id;
```

这条语句和普通明细查询最大的区别是：

1. 它不再返回每条传感器记录本身；
2. 而是按 `device_id` 分组后，为每个设备返回一条统计结果。

换句话说，结果集中的每一行表示的是：

> 某一个设备这一组数据的统计摘要。

所以这里的列读取含义也发生了变化：

1. 第 0 列：设备 ID
2. 第 1 列：该设备的数据条数 `COUNT(*)`
3. 第 2 列：平均温度 `AVG(...)`
4. 第 3 列：最低温度 `MIN(...)`
5. 第 4 列：最高温度 `MAX(...)`

这说明一个很重要的原则：

> `sqlite3_column_*()` 读取的是“查询结果列”，而不是“表字段原样拷贝”。

只要你的 SQL 使用了表达式、别名、聚合函数，那么读取时就应该按照“结果列定义”去理解，而不是只盯着原表结构。

### 深入理解：回调查询是 `sqlite3_exec()` 的另一种读取方式

示例最后演示了：

```cpp
sqlite3_exec(db, "SELECT * FROM devices WHERE type = 'sensor';",
             callback, nullptr, &err_msg);
```

它的特点是：

1. 仍然通过 `sqlite3_exec()` 传入 SQL；
2. 每得到一行结果，就自动调用一次回调函数；
3. 这一行的列值和列名会以 `char**` 的形式传给回调。

对应的回调函数签名通常是：

```cpp
int callback(void *data, int argc, char **argv, char **col_names)
```

其中：

1. `argc`：当前行有多少列；
2. `argv[i]`：第 `i` 列的字符串形式值；
3. `col_names[i]`：第 `i` 列的列名。

这种方式的优点是写起来短，适合：

1. 快速验证 SQL 是否有结果；
2. 小型工具脚本；
3. 教学中演示“查询返回了哪些列和值”。

但它也有明显限制：

1. 所有值都以字符串指针形式传入，不如 `sqlite3_column_int/double/text()` 那样类型明确；
2. 对复杂业务逻辑不够直观；
3. 不方便像预编译语句那样做参数绑定和语句复用。

因此在实际工程中：

1. 简单一次性查询，可以考虑 `sqlite3_exec()` + 回调；
2. 稍复杂、需要参数化、需要类型化读取的查询，优先使用 `sqlite3_prepare_v2()` 这一套接口。

### 小结：查询操作的推荐心智模型

可以把查询理解为“数据库在按行喂数据给你”：

1. `prepare`：告诉 SQLite 你要查什么；
2. `bind`：补上条件参数；
3. `step`：向数据库要下一行；
4. `column`：从当前行取各列值；
5. 重复直到 `SQLITE_DONE`；
6. `finalize`：释放语句对象。

只要把这套模式掌握住，后面无论是简单查询、聚合统计，还是更复杂的多表查询，阅读和编写 SQLite C API 代码都会容易很多。

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

### 深入理解：`UPDATE` 和 `DELETE` 与 `INSERT` 本质上是同一套 API 模式

虽然 SQL 语义不同，但从 SQLite C API 的角度看，`UPDATE`、`DELETE` 和前面讲过的参数化 `INSERT` 基本属于同一类操作。

它们的共同点是：

1. 一般不返回结果集明细；
2. 都可以写成带 `?` 占位符的参数化 SQL；
3. 都通过 `sqlite3_prepare_v2()` + `sqlite3_bind_*()` + `sqlite3_step()` 执行；
4. 成功执行完成时，`sqlite3_step()` 通常返回 `SQLITE_DONE`。

也就是说，你可以把它们统一理解为：

> 先准备一条“修改数据库状态”的命令模板，再把本次要修改的值绑定进去，然后执行。

例如示例中的更新语句：

```cpp
sqlite3_prepare_v2(db,
    "UPDATE devices SET ip_address = ? WHERE device_id = ?;",
    -1, &stmt, nullptr);
sqlite3_bind_text(stmt, 1, "10.0.0.101", -1, SQLITE_STATIC);
sqlite3_bind_int(stmt, 2, 1);
sqlite3_step(stmt);
```

它和前面 `INSERT INTO ... VALUES (?, ?, ?)` 的 API 使用方式完全一致，只是 SQL 模板换成了 `UPDATE`。

### 深入理解：`UPDATE` 的语义是“找到行，再修改列值”

下面这条 SQL：

```sql
UPDATE devices SET ip_address = ? WHERE device_id = ?;
```

可以拆成两部分来理解：

1. **`SET ip_address = ?`**：把匹配到的行的 `ip_address` 列改成新值；
2. **`WHERE device_id = ?`**：只修改 `device_id` 等于指定值的那一行。

也就是说，`UPDATE` 并不是“创建新行”，而是：

> 在已有记录中找出符合条件的行，并在原地修改其列值。

这也是为什么更新语句最核心的部分通常不是 `SET`，而是 `WHERE`。`SET` 决定“改什么”，`WHERE` 决定“改谁”。

### 深入理解：为什么 `UPDATE` 和 `DELETE` 必须反复强调 `WHERE`

文档前面的 SQL 语法已经给出警告：不加 `WHERE`，会影响所有行。

这是数据库操作里最常见、也最危险的错误之一。

例如：

```sql
UPDATE devices SET type='unknown';
```

含义不是“改某一台设备”，而是：

> 把 `devices` 表中的每一行的 `type` 都改成 `unknown`。

同理：

```sql
DELETE FROM sensor_data;
```

含义不是“删一条数据”，而是：

> 删除 `sensor_data` 表中的所有记录。

因此在工程实践中，有两个很重要的心智习惯：

1. 写 `UPDATE` / `DELETE` 时，先想清楚“我要影响哪些行”；
2. 在真正执行前，先检查 `WHERE` 条件是否足够精确。

如果是重要数据，很多开发者会先写一条同条件的 `SELECT` 来确认范围，例如：

```sql
SELECT * FROM devices WHERE type = 'sensor';
```

确认命中的确实是你要修改或删除的那些行后，再把 `SELECT *` 改成 `UPDATE ...` 或 `DELETE ...`。

### 深入理解：为什么更新和删除也要使用参数绑定

有些初学者会觉得只有插入用户输入时才需要参数绑定，更新和删除似乎可以直接拼 SQL。实际上不是这样。

例如示例中的删除操作：

```cpp
sqlite3_prepare_v2(db,
    "DELETE FROM devices WHERE name = ?;", -1, &stmt, nullptr);
sqlite3_bind_text(stmt, 1, "旧设备", -1, SQLITE_STATIC);
```

这里使用参数绑定有三个直接好处：

1. **安全**：避免外部输入污染 SQL 结构；
2. **正确**：不用手工处理引号、转义等字符串细节；
3. **复用**：同一个语句模板可以删除不同名称的设备。

所以一个值得建立的习惯是：

> 只要 SQL 条件里有变量值，就优先考虑写成参数化语句。

不管它是 `INSERT`、`SELECT`、`UPDATE` 还是 `DELETE`。

### 深入理解：`sqlite3_changes()` 表示“刚刚那条语句影响了多少行”

示例中在执行完更新或删除之后，会立即打印：

```cpp
sqlite3_changes(db)
```

它返回的是：

> 在当前数据库连接上，最近一次完成的 `INSERT`、`UPDATE` 或 `DELETE` 语句影响的行数。

例如：

1. 更新 `device_id = 1` 的 IP，如果成功命中 1 行，则返回 1；
2. 批量把所有 `sensor` 改为 `temp_sensor`，如果命中 3 行，则返回 3；
3. 删除名为“旧设备”的记录，如果删掉 1 行，则返回 1。

它非常适合做两类事情：

1. 输出日志，确认这次操作影响了几行；
2. 做最基本的业务判断，例如“如果影响行数为 0，说明目标记录不存在”。

例如：

```cpp
if (sqlite3_changes(db) == 0) {
    printf("没有匹配到任何设备，可能设备不存在。\n");
}
```

### 深入理解：`sqlite3_total_changes()` 表示“这个连接累计改过多少行”

示例最后还打印了：

```cpp
sqlite3_total_changes(db)
```

它和 `sqlite3_changes(db)` 的区别是：

1. `sqlite3_changes(db)` 看的是“最近一次语句”的影响行数；
2. `sqlite3_total_changes(db)` 看的是“这个数据库连接自打开以来累计影响的总行数”。

你可以把它们理解成：

| 函数 | 观察范围 |
|------|----------|
| `sqlite3_changes()` | 单次最近语句 |
| `sqlite3_total_changes()` | 当前连接整个生命周期 |

以本示例为例，前面既插入了测试数据，又做了更新和删除，所以最后的累计值通常会大于单次 `UPDATE` 或 `DELETE` 的返回值。

这两个函数在调试时很有帮助，尤其适合确认：

1. 语句到底有没有真正生效；
2. 本次连接一共做了多少数据修改。

### 深入理解：为什么批量更新可以直接用 `sqlite3_exec()`

示例中的第 2 个更新操作写成了：

```cpp
exec_sql(db, "UPDATE devices SET type = 'temp_sensor' WHERE type = 'sensor';");
```

这里没有用 `prepare + bind`，而是直接 `sqlite3_exec()`，这是因为这条 SQL：

1. 本身不需要外部参数；
2. 只执行一次；
3. 也不需要读取结果集。

因此用 `sqlite3_exec()` 会更简洁。

这也说明一个实用原则：

1. **无参数、一次性、无结果集** 的 SQL，可以优先考虑 `sqlite3_exec()`；
2. **有参数、要复用、或需要更精细控制** 的 SQL，优先用 `sqlite3_prepare_v2()` 这一套接口。

这不是“哪一个绝对更高级”，而是要按场景选合适的工具。

### 深入理解：删除操作的语义是“移除行”，不是“把字段清空”

这一点对初学者也很重要。

```sql
DELETE FROM devices WHERE name = ?;
```

它的含义是：

> 把满足条件的整行记录从表中移除。

删除后，这一行不再存在，后续查询也查不到它。它不是把某一列改成 `NULL`，也不是做“逻辑删除”。

如果你只是想把某个字段内容清空，更接近的操作应当是：

```sql
UPDATE devices SET ip_address = NULL WHERE device_id = 1;
```

所以：

1. `UPDATE` 是“保留这行，只改内容”；
2. `DELETE` 是“把整行移除”。

### 补充：外键存在时，删除操作还要考虑引用关系

本章前面在 `sensor_data` 表中定义过：

```sql
FOREIGN KEY (device_id) REFERENCES devices(device_id)
```

这意味着 `sensor_data` 的记录依赖于 `devices` 中的设备主键。换句话说，`devices` 是父表，`sensor_data` 是子表，后者不能脱离前者独立成立。

在更完整的数据库设计里，这会带来一个重要问题：

> 如果某个设备还有相关传感器数据，是否允许直接删除这台设备？

答案取决于两件事：

1. 当前 SQLite 连接是否真的开启了外键检查。
2. 这条外键定义是否额外配置了 `ON DELETE` 策略。

如果外键检查没有开启，那么即使表结构里写了 `FOREIGN KEY`，SQLite 也可能允许你删掉父表记录，最终留下“子表还在、父表没了”的不一致数据。

如果外键检查已经开启，那么行为通常由外键策略决定。例如：

1. 有的设计会禁止删除被引用的父表记录；
2. 有的设计会在删除父表记录时级联删除子表记录；
3. 有的设计会把子表引用字段置空。

例如下面这条定义：

```sql
FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
```

它的含义就是：删除某台设备时，引用这台设备的 `sensor_data` 记录也一起删除。

而如果你写的是：

```sql
FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE RESTRICT
```

那么当这台设备仍然被传感器数据引用时，删除操作会直接失败。

所以在真实项目里，删除父表记录之前，必须先回答一个业务问题：

> 这条父记录被删掉后，依赖它的子记录应该被拒绝、级联删除，还是改成空引用？

本章示例主要聚焦 CRUD 基本语法，没有在代码里完整展开外键删除策略，但你现在应该把它当成表设计的一部分，而不是“删除时报错了再临时处理”。

### 深入理解：更新和删除后的验证为什么很重要

示例中每做完一次 `UPDATE` 或 `DELETE`，都会再次调用 `print_devices()` 输出当前表内容。这其实是在做一个非常好的工程习惯演示：

> 修改数据库之后，不要只相信“语句执行成功”，而要验证“结果是否真的符合预期”。

这种验证通常可以分成两层：

1. **接口层验证**：检查 `sqlite3_step()` 是否成功、`sqlite3_changes()` 是否符合预期；
2. **结果层验证**：重新查询数据，确认被修改或删除的记录确实变成了想要的状态。

对教学代码来说，这样能帮助读者把“执行 SQL”和“观察结果”联系起来；对生产代码来说，这样的思想也同样重要。

### 小结：更新和删除操作的推荐心智模型

可以把 `UPDATE` 和 `DELETE` 理解成“按条件命中记录，再改变数据库状态”：

1. 先写清楚 `WHERE`，明确影响范围；
2. 有变量值时优先使用参数绑定；
3. 用 `sqlite3_step()` 执行，成功通常得到 `SQLITE_DONE`；
4. 用 `sqlite3_changes()` 看这次到底影响了多少行；
5. 必要时重新查询，验证结果确实正确。

一旦掌握了这套思路，你会发现 `INSERT`、`UPDATE`、`DELETE` 在 C API 层面其实非常统一，真正变化的主要只是 SQL 本身的业务含义。

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
- 在 SQLite 中声明了外键后，若要真正执行外键约束检查，通常还需要在每个连接上显式开启 `PRAGMA foreign_keys = ON;`

### 一张图看懂本章：SQLite C API 的统一工作流

虽然本章分别讲了 `CREATE`、`INSERT`、`SELECT`、`UPDATE`、`DELETE`，但如果从 C API 的角度去看，它们其实可以归纳成两大类工作流。

#### 第一类：只执行，不读取结果

适用场景：

1. `CREATE TABLE`
2. 不带返回结果的 `INSERT`
3. `UPDATE`
4. `DELETE`
5. 其他不需要逐行读取结果的 SQL

最简单时可以直接用：

```cpp
sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
```

如果语句里有参数，或希望复用语句对象，则使用：

```cpp
sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
sqlite3_bind_*(stmt, ...);
sqlite3_step(stmt);        // 成功通常返回 SQLITE_DONE
sqlite3_finalize(stmt);
```

#### 第二类：执行后还要逐行读取结果

适用场景：

1. `SELECT`
2. `PRAGMA table_info(...)`
3. 查询 `sqlite_master`
4. 其他会返回结果集的语句

典型模式是：

```cpp
sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
sqlite3_bind_*(stmt, ...);   // 如果有参数

while (sqlite3_step(stmt) == SQLITE_ROW) {
    sqlite3_column_*(stmt, col);
}

sqlite3_finalize(stmt);
```

这两类模式几乎覆盖了本章全部示例。真正变化的主要不是 API 结构，而是 SQL 本身的业务含义。

### 本章最重要的 4 个 API 生命周期

如果要把第三章压缩成最核心的记忆点，可以记住下面 4 个函数阶段：

1. **`sqlite3_prepare_v2()`**：把 SQL 文本编译成可执行语句对象。
2. **`sqlite3_bind_*()`**：给占位符 `?` 传入本次实际参数。
3. **`sqlite3_step()`**：真正推动 SQL 执行，或取下一行结果。
4. **`sqlite3_finalize()`**：释放语句对象，回收资源。

如果要复用语句对象，再额外记住两个辅助函数：

1. **`sqlite3_reset()`**：重置语句执行状态。
2. **`sqlite3_clear_bindings()`**：清空上一次绑定的参数值。

你可以把这整套流程记成一句话：

> `prepare -> bind -> step -> (column / reset) -> finalize`

其中：

1. 查询语句会在 `step` 后配合 `column` 读数据；
2. 批量插入或重复执行时，会在 `step` 后配合 `reset` 和 `clear_bindings` 复用语句。

### 本章最容易混淆的 5 个细节

这部分很适合在做练习或写代码时反复对照。

#### 1. 参数索引从 1 开始，结果列索引从 0 开始

这是 SQLite C API 最经典的易错点：

1. `sqlite3_bind_text(stmt, 1, ...)` 里的 `1` 表示第一个占位符；
2. `sqlite3_column_text(stmt, 0)` 里的 `0` 表示第一列结果。

#### 2. `sqlite3_step()` 在不同语句里的含义不同

1. 对 `INSERT/UPDATE/DELETE`，成功通常得到 `SQLITE_DONE`；
2. 对 `SELECT`，每取到一行结果就返回 `SQLITE_ROW`；
3. 查询结果读完后，才会返回 `SQLITE_DONE`。

#### 3. `sqlite3_column_*()` 读取的是“结果列”，不是“表字段原样顺序”

如果 SQL 用了别名、表达式、聚合函数，那么读取时应该按 `SELECT` 结果列去理解，不要只盯着建表字段顺序。

#### 4. `NULL` 需要单独处理

尤其是 `sqlite3_column_text()` 返回空指针时，不能直接当 C 字符串使用。必要时先用 `sqlite3_column_type()` 判断是否为 `SQLITE_NULL`。

#### 5. `sqlite3_changes()` 和 `sqlite3_total_changes()` 不是一回事

1. `sqlite3_changes()` 看最近一次语句影响了多少行；
2. `sqlite3_total_changes()` 看当前连接累计影响了多少行。

### 本章最重要的工程习惯

如果你不只是在学语法，而是准备写实际可维护的数据库代码，那么下面这些习惯比记住单条 SQL 语法更重要。

#### 1. 任何外部输入都不要拼 SQL

统一使用参数绑定。这样可以同时解决：

1. SQL 注入风险；
2. 引号和转义问题；
3. 语句复用和结构清晰问题。

#### 2. 修改数据前先明确影响范围

写 `UPDATE`、`DELETE` 时，先确认 `WHERE` 是否精确。必要时先用同条件 `SELECT` 验证命中范围。

#### 3. 执行成功不等于结果正确

除了检查返回码，还要看：

1. `sqlite3_changes()` 是否符合预期；
2. 重新查询后的结果是否正确；
3. 是否真的命中了你想操作的记录。

#### 4. 有重复执行需求时，优先考虑语句复用

尤其是批量插入、批量更新、循环查询等场景，使用预编译语句对象会更高效，也更容易维护。

### 从本章过渡到下一章时，你应该已经掌握什么

如果第三章内容已经吃透，那么你应该已经具备下面这些能力：

1. 能创建表，并理解主键、默认值、非空约束和外键的基本作用。
2. 能安全地插入数据，并知道为什么要使用参数绑定。
3. 能逐行读取查询结果，并正确处理列索引、类型和空值。
4. 能执行更新和删除，并理解 `WHERE` 条件的重要性。
5. 能看懂大多数 SQLite C API 的基本执行流程。

做到这一点后，下一章的高级查询就不再只是“记 SQL 语法”，而是在已有 CRUD 基础上继续叠加：

1. 更复杂的筛选条件；
2. 多表关联；
3. 子查询、分组、排序等更强的查询表达能力。

### 最后总结：把第三章浓缩成一句话

如果只用一句话概括本章，可以记成：

> 用 SQL 描述你想操作的数据，用 `prepare/bind/step/column/finalize` 驱动 SQLite 执行，并始终验证结果是否符合预期。

---

[⬅ 第二章：SQLite 简介](ch02_sqlite_intro.md) | [目录](../README.md) | [第四章：高级查询 ➡](ch04_advanced_query.md)
