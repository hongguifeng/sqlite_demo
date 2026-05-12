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

    // UPDATE：修改特定设备的 IP
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

    // UPDATE：批量修改
    exec_sql(db, "UPDATE devices SET type = 'temp_sensor' WHERE type = 'sensor';");
    printf("UPDATE: 影响了 %d 行\n", sqlite3_changes(db));
    print_devices(db, "批量更新 type 后");

    // DELETE：删除特定设备
    sqlite3_prepare_v2(db,
        "DELETE FROM devices WHERE name = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, "旧设备", -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    printf("\nDELETE: 影响了 %d 行\n", sqlite3_changes(db));
    sqlite3_finalize(stmt);
    print_devices(db, "删除 '旧设备' 后");

    printf("\n本次连接总共修改了 %d 行\n", sqlite3_total_changes(db));

    sqlite3_close(db);
    remove("update_delete_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
