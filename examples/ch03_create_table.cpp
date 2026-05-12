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

    // 创建设备表
    const char *sql_create_devices =
        "CREATE TABLE IF NOT EXISTS devices ("
        "    device_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name        TEXT NOT NULL,"
        "    type        TEXT NOT NULL DEFAULT 'sensor',"
        "    ip_address  TEXT,"
        "    created_at  TEXT DEFAULT (datetime('now', 'localtime'))"
        ");";
    exec_sql(db, sql_create_devices, "创建 devices 表");

    // 创建传感器数据表
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

    // 查看已创建的表
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

    // 查看 devices 表结构
    printf("\n--- devices 表结构 ---\n");
    rc = sqlite3_prepare_v2(db,
        "PRAGMA table_info(devices);", -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  列: %-15s 类型: %-10s 非空: %d 默认: %s\n",
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_int(stmt, 3),
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
