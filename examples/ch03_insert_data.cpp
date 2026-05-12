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

    // 方法1：直接执行 SQL
    printf("--- 方法1: 直接执行 SQL ---\n");
    exec_sql(db,
        "INSERT INTO devices (name, type, ip_address) "
        "VALUES ('温度传感器A', 'sensor', '192.168.1.101');");
    printf("插入设备 1，rowid = %lld\n", sqlite3_last_insert_rowid(db));

    // 方法2：预编译语句 + 参数绑定
    printf("\n--- 方法2: 预编译语句 + 参数绑定 ---\n");

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "INSERT INTO devices (name, type, ip_address) VALUES (?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "预编译失败: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sqlite3_bind_text(stmt, 1, "温度传感器B", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "sensor", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "192.168.1.102", -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        printf("插入设备 2，rowid = %lld\n", sqlite3_last_insert_rowid(db));
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    sqlite3_bind_text(stmt, 1, "网关设备", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "gateway", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "192.168.1.1", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    printf("插入设备 3，rowid = %lld\n", sqlite3_last_insert_rowid(db));

    sqlite3_finalize(stmt);

    // 批量插入传感器数据
    printf("\n--- 批量插入传感器数据 ---\n");

    const char *sql_sensor =
        "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);";
    sqlite3_prepare_v2(db, sql_sensor, -1, &stmt, nullptr);

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

    // 验证
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
