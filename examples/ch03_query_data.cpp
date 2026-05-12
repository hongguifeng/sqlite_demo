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

    // 1. 查询所有设备
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

    // 2. 条件查询
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

    // 3. 排序 + LIMIT
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

    // 4. 聚合查询
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

    // 5. 使用回调方式查询
    printf("\n=== 5. 使用回调方式查询 ===\n");
    auto callback = [](void *, int argc, char **argv, char **col_names) -> int {
        for (int i = 0; i < argc; i++) {
            printf("  %s = %s", col_names[i], argv[i] ? argv[i] : "NULL");
        }
        printf("\n");
        return 0;
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
