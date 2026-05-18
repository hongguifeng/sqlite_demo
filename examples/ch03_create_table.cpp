#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>

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

    // 创建设备表。
    // 这张表用于保存设备基础信息，可以把它理解为“设备档案表”。
    // CREATE TABLE IF NOT EXISTS 的含义是：如果表不存在就创建，已存在则跳过，不报错。
    // 各字段含义如下：
    // 1. device_id：设备主键，INTEGER PRIMARY KEY AUTOINCREMENT 表示主键整数自动递增；
    // 2. name：设备名称，TEXT NOT NULL 表示必须提供名称；
    // 3. type：设备类型，默认值为 'sensor'，如果插入时不写就自动使用该值；
    // 4. ip_address：设备 IP 地址，没有 NOT NULL 约束，因此可以为空；
    // 5. created_at：创建设备记录的时间，默认使用当前本地时间。
    // 其中 datetime('now', 'localtime') 是 SQLite 的日期时间函数，
    // 表示取当前时间并按本地时区格式化成文本。
    const char *sql_create_devices =
        "CREATE TABLE IF NOT EXISTS devices ("
        "    device_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name        TEXT NOT NULL,"
        "    type        TEXT NOT NULL DEFAULT 'sensor',"
        "    ip_address  TEXT,"
        "    created_at  TEXT DEFAULT (datetime('now', 'localtime'))"
        ");";
    exec_sql(db, sql_create_devices, "创建 devices 表");

    // 创建传感器数据表。
    // 这张表用于保存设备采集到的温湿度读数，一条记录通常对应某个时刻的一次采样。
    // 它和 devices 表之间通过 device_id 建立关联，表示“这条数据属于哪个设备”。
    // 各字段含义如下：
    // 1. id：当前采样记录的主键，自增；
    // 2. device_id：所属设备 ID，不允许为空；
    // 3. temperature：温度值，REAL 适合存放浮点数；
    // 4. humidity：湿度值，同样使用 REAL；
    // 5. timestamp：采样时间，默认使用当前本地时间；
    // 6. FOREIGN KEY (device_id) REFERENCES devices(device_id)：外键约束，
    //    表示 sensor_data.device_id 应该引用 devices 表中已存在的 device_id。
    // 这样设计后，devices 是“主表”，sensor_data 是“明细表”或“子表”，
    // 便于后续做按设备查询、关联查询和统计分析。
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

    // 查看已创建的表。
    // sqlite_master 是 SQLite 自动维护的系统表，里面保存了当前数据库中各种对象的定义信息，
    // 例如表(table)、索引(index)、视图(view)、触发器(trigger)等。
    // 这条语句的含义可以拆开看：
    // 1. SELECT name：只取对象名称这一列；
    // 2. FROM sqlite_master：数据来源是 SQLite 的系统元数据表；
    // 3. WHERE type='table'：只保留类型为 table 的对象，也就是数据库中的表；
    // 4. ORDER BY name：按表名升序排列，便于稳定、直观地输出结果。
    // 执行后通常会看到我们刚创建的 devices 和 sensor_data，
    // 以及 SQLite 为带 AUTOINCREMENT 的表自动维护的 sqlite_sequence 等内部表。
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

    // 查看 devices 表结构。
    // PRAGMA 是 SQLite 提供的一类特殊命令，用来查询数据库的内部信息或调整数据库行为。
    // 这里的 PRAGMA table_info(devices) 用于读取 devices 表的列定义，
    // 相当于让 SQLite 告诉我们“这个表有哪些列、每列是什么类型、有哪些约束”。
    // 其中 table_info 可以直译为“表信息”，devices 表示要查看的目标表名。
    // 该命令返回的每一行对应表中的一列，常见字段含义如下：
    // 1. cid：列的编号，从 0 开始；
    // 2. name：列名；
    // 3. type：声明的数据类型；
    // 4. notnull：是否声明了 NOT NULL，1 表示是，0 表示否；
    // 5. dflt_value：默认值；
    // 6. pk：是否属于主键，非 0 表示该列是主键的一部分。
    // 在下面的输出中，我们实际取用了第 1、2、3、4 列索引对应的数据，
    // 用来展示列名、类型、非空约束和默认值，从而验证建表语句是否按预期生效。
    // 注意：这里的“第 1、2、3、4 列”说的是查询结果列下标，sqlite3_column_* 的列索引从 0 开始。
    printf("\n--- devices 表结构 ---\n");
    rc = sqlite3_prepare_v2(db,
        // 读取 devices 表的元数据；返回结果不是业务数据，而是表结构说明。
        "PRAGMA table_info(devices);", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  列: %-15s 类型: %-10s 非空: %d 默认: %s\n",
                sqlite3_column_text(stmt, 1),   // 第 1 列：name，列名
                sqlite3_column_text(stmt, 2),   // 第 2 列：type，声明的数据类型
                sqlite3_column_int(stmt, 3),    // 第 3 列：notnull，是否声明 NOT NULL
                sqlite3_column_text(stmt, 4) ?
                    (const char*)sqlite3_column_text(stmt, 4) : "(无)"); // 第 4 列：dflt_value，默认值
        }
    }
    sqlite3_finalize(stmt);

    // 查看当前连接是否已开启外键检查。
    printf("\n--- foreign_keys 开关状态 ---\n");
    rc = sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  foreign_keys = %d\n", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // 查看 sensor_data 表声明了哪些外键。
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
