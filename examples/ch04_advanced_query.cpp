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

static void prepare_data(sqlite3 *db) {
    exec_sql(db,
        "CREATE TABLE devices ("
        "  device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL, type TEXT NOT NULL, ip_address TEXT);");
    exec_sql(db,
        "CREATE TABLE sensor_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id INTEGER NOT NULL,"
        "  temperature REAL, humidity REAL,"
        "  timestamp TEXT DEFAULT (datetime('now','localtime')),"
        "  FOREIGN KEY(device_id) REFERENCES devices(device_id));");

    exec_sql(db,
        "INSERT INTO devices (name, type, ip_address) VALUES "
        "('温度传感器A', 'sensor', '192.168.1.101'),"
        "('温度传感器B', 'sensor', '192.168.1.102'),"
        "('网关设备', 'gateway', '192.168.1.1');");

    exec_sql(db,
        "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES "
        "(1, 25.3, 60.2),(1, 25.5, 59.8),(1, 25.1, 61.0),(1, 26.7, 58.5),(1, 27.1, 57.0),"
        "(2, 24.1, 62.0),(2, 24.3, 61.5),(2, 24.0, 62.3),(2, 23.5, 63.8);");
}

static void run_query(sqlite3 *db, const char *title, const char *sql) {
    printf("\n=== %s ===\n", title);
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "  预编译失败: %s\n", sqlite3_errmsg(db));
        return;
    }

    // 打印列名
    int col_count = sqlite3_column_count(stmt);
    printf("  ");
    for (int i = 0; i < col_count; i++) {
        printf("%-16s", sqlite3_column_name(stmt, i));
    }
    printf("\n  ");
    for (int i = 0; i < col_count; i++) {
        printf("%-16s", "────────────────");
    }
    printf("\n");

    // 打印数据
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  ");
        for (int i = 0; i < col_count; i++) {
            const char *val = (const char*)sqlite3_column_text(stmt, i);
            printf("%-16s", val ? val : "NULL");
        }
        printf("\n");
    }
    sqlite3_finalize(stmt);
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("advanced_query_demo.db", &db);
    prepare_data(db);

    // ==========================================
    // 1. INNER JOIN
    // ==========================================
    run_query(db, "1. INNER JOIN: 传感器数据 + 设备名称",
        "SELECT d.name, s.temperature, s.humidity "
        "FROM sensor_data s "
        "INNER JOIN devices d ON s.device_id = d.device_id "
        "ORDER BY s.temperature DESC;");

    // ==========================================
    // 2. LEFT JOIN: 包含无数据的设备
    // ==========================================
    run_query(db, "2. LEFT JOIN: 所有设备及数据条数",
        "SELECT d.name, d.type, COUNT(s.id) as data_count, "
        "ROUND(AVG(s.temperature), 1) as avg_temp "
        "FROM devices d "
        "LEFT JOIN sensor_data s ON d.device_id = s.device_id "
        "GROUP BY d.device_id;");

    // ==========================================
    // 3. 子查询: 温度高于平均值的记录
    // ==========================================
    run_query(db, "3. 子查询: 温度高于平均值",
        "SELECT device_id, temperature, humidity "
        "FROM sensor_data "
        "WHERE temperature > (SELECT AVG(temperature) FROM sensor_data);");

    // ==========================================
    // 4. 子查询: 有数据的设备
    // ==========================================
    run_query(db, "4. 子查询: 有传感器数据的设备",
        "SELECT * FROM devices "
        "WHERE device_id IN (SELECT DISTINCT device_id FROM sensor_data);");

    // ==========================================
    // 5. 创建索引并查看执行计划
    // ==========================================
    printf("\n=== 5. 索引与执行计划 ===\n");

    // 无索引时的执行计划
    printf("  无索引时:\n");
    run_query(db, "EXPLAIN (无索引)",
        "EXPLAIN QUERY PLAN SELECT * FROM sensor_data WHERE device_id = 1;");

    // 创建索引
    exec_sql(db, "CREATE INDEX idx_sensor_device_id ON sensor_data(device_id);");
    printf("  已创建索引 idx_sensor_device_id\n");

    // 有索引时的执行计划
    run_query(db, "EXPLAIN (有索引)",
        "EXPLAIN QUERY PLAN SELECT * FROM sensor_data WHERE device_id = 1;");

    // ==========================================
    // 6. 创建和使用视图
    // ==========================================
    exec_sql(db,
        "CREATE VIEW device_summary AS "
        "SELECT d.device_id, d.name, d.type, "
        "COUNT(s.id) as total_readings, "
        "ROUND(AVG(s.temperature), 1) as avg_temp, "
        "ROUND(MAX(s.temperature), 1) as max_temp, "
        "ROUND(MIN(s.temperature), 1) as min_temp "
        "FROM devices d "
        "LEFT JOIN sensor_data s ON d.device_id = s.device_id "
        "GROUP BY d.device_id;");

    run_query(db, "6. 视图: device_summary",
        "SELECT * FROM device_summary;");

    // ==========================================
    // 7. HAVING: 筛选分组后的结果
    // ==========================================
    run_query(db, "7. HAVING: 平均温度 > 25 的设备",
        "SELECT d.name, ROUND(AVG(s.temperature), 1) as avg_temp, COUNT(*) as cnt "
        "FROM sensor_data s "
        "JOIN devices d ON s.device_id = d.device_id "
        "GROUP BY s.device_id "
        "HAVING avg_temp > 25.0;");

    // ==========================================
    // 8. CASE 表达式
    // ==========================================
    run_query(db, "8. CASE: 温度等级分类",
        "SELECT device_id, temperature, "
        "CASE "
        "  WHEN temperature >= 27.0 THEN '高温' "
        "  WHEN temperature >= 25.0 THEN '正常' "
        "  ELSE '低温' "
        "END as level "
        "FROM sensor_data "
        "ORDER BY temperature DESC;");

    sqlite3_close(db);
    remove("advanced_query_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
