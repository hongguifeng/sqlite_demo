#include <sqlite3.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>

// ==========================================
// 嵌入式传感器数据管理系统
// ==========================================

class SensorDatabase {
public:
    bool open(const char *path) {
        int rc = sqlite3_open_v2(path, &db_,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db_));
            return false;
        }
        // 嵌入式推荐配置
        exec("PRAGMA journal_mode = WAL;");
        exec("PRAGMA synchronous = NORMAL;");
        exec("PRAGMA cache_size = -2000;");  // 2MB 缓存
        exec("PRAGMA foreign_keys = ON;");
        exec("PRAGMA busy_timeout = 5000;");
        return initTables();
    }

    void close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    ~SensorDatabase() { close(); }

    // --- 设备管理 ---
    int addDevice(const char *name, const char *type, const char *location) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO devices (name, type, location, status) VALUES (?, ?, ?, 'online');",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, location, -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db_) : -1;
    }

    bool setDeviceStatus(int device_id, const char *status) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "UPDATE devices SET status = ? WHERE device_id = ?;",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, device_id);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // --- 数据采集 ---
    bool insertReading(int device_id, double temperature, double humidity) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, device_id);
        sqlite3_bind_double(stmt, 2, temperature);
        sqlite3_bind_double(stmt, 3, humidity);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_DONE;
    }

    // 批量插入（使用事务）
    int insertBatch(int device_id, const double *temps, const double *humids, int count) {
        exec("BEGIN TRANSACTION;");
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        int inserted = 0;
        for (int i = 0; i < count; i++) {
            sqlite3_bind_int(stmt, 1, device_id);
            sqlite3_bind_double(stmt, 2, temps[i]);
            sqlite3_bind_double(stmt, 3, humids[i]);
            if (sqlite3_step(stmt) == SQLITE_DONE) inserted++;
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
        sqlite3_finalize(stmt);
        exec("COMMIT;");
        return inserted;
    }

    // --- 告警 ---
    int addAlertRule(int device_id, const char *metric,
                     const char *condition, double threshold) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO alert_rules (device_id, metric, condition, threshold) "
            "VALUES (?, ?, ?, ?);", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, device_id);
        sqlite3_bind_text(stmt, 2, metric, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, condition, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 4, threshold);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db_) : -1;
    }

    int checkAndRecordAlerts(int device_id, double temperature, double humidity) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT id, metric, condition, threshold FROM alert_rules "
            "WHERE device_id = ? AND enabled = 1;", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, device_id);

        int alert_count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int rule_id = sqlite3_column_int(stmt, 0);
            const char *metric = (const char*)sqlite3_column_text(stmt, 1);
            const char *cond = (const char*)sqlite3_column_text(stmt, 2);
            double threshold = sqlite3_column_double(stmt, 3);

            double value = 0;
            if (strcmp(metric, "temperature") == 0) value = temperature;
            else if (strcmp(metric, "humidity") == 0) value = humidity;

            bool triggered = false;
            if (strcmp(cond, ">") == 0) triggered = (value > threshold);
            else if (strcmp(cond, "<") == 0) triggered = (value < threshold);
            else if (strcmp(cond, ">=") == 0) triggered = (value >= threshold);
            else if (strcmp(cond, "<=") == 0) triggered = (value <= threshold);

            if (triggered) {
                recordAlert(rule_id, device_id, value);
                alert_count++;
            }
        }
        sqlite3_finalize(stmt);
        return alert_count;
    }

    // --- 查询与统计 ---
    void printDeviceSummary() {
        printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
        printf("│                      设备数据概览                                │\n");
        printf("├────┬──────────────┬──────┬─────┬──────┬──────┬──────┬───────────┤\n");
        printf("│ ID │ 名称         │ 状态  │ 条数 │ 平均T │ 最高T │ 最低T │ 告警次数   │\n");
        printf("├────┼──────────────┼──────┼─────┼──────┼──────┼──────┼───────────┤\n");

        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT d.device_id, d.name, d.status, "
            "COUNT(s.id), ROUND(AVG(s.temperature),1), "
            "ROUND(MAX(s.temperature),1), ROUND(MIN(s.temperature),1), "
            "(SELECT COUNT(*) FROM alert_events ae WHERE ae.device_id = d.device_id) "
            "FROM devices d "
            "LEFT JOIN sensor_data s ON d.device_id = s.device_id "
            "GROUP BY d.device_id;", -1, &stmt, nullptr);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("│ %-2d │ %-12s │ %-4s │ %-3d │ %-4s │ %-4s │ %-4s │ %-9d │\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : "N/A",
                sqlite3_column_text(stmt, 5) ? (const char*)sqlite3_column_text(stmt, 5) : "N/A",
                sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : "N/A",
                sqlite3_column_int(stmt, 7));
        }
        sqlite3_finalize(stmt);
        printf("└────┴──────────────┴──────┴─────┴──────┴──────┴──────┴───────────┘\n");
    }

    void printAlertEvents() {
        printf("\n--- 告警记录 ---\n");
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT ae.id, d.name, ar.metric, ar.condition, ar.threshold, "
            "ae.value, ae.timestamp "
            "FROM alert_events ae "
            "JOIN devices d ON ae.device_id = d.device_id "
            "JOIN alert_rules ar ON ae.rule_id = ar.id "
            "ORDER BY ae.timestamp DESC LIMIT 10;",
            -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  #%d [%s] %s %s %.1f → 实际值: %.1f @ %s\n",
                sqlite3_column_int(stmt, 0),
                sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3),
                sqlite3_column_double(stmt, 4),
                sqlite3_column_double(stmt, 5),
                sqlite3_column_text(stmt, 6));
        }
        sqlite3_finalize(stmt);
    }

    // --- 维护 ---
    int cleanOldData(int keep_days) {
        char sql[256];
        snprintf(sql, sizeof(sql),
            "DELETE FROM sensor_data WHERE timestamp < datetime('now', '-%d days');",
            keep_days);
        exec(sql);
        return sqlite3_changes(db_);
    }

    bool integrityCheck() {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_, "PRAGMA integrity_check;", -1, &stmt, nullptr);
        bool ok = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            ok = (strcmp((const char*)sqlite3_column_text(stmt, 0), "ok") == 0);
        }
        sqlite3_finalize(stmt);
        return ok;
    }

private:
    sqlite3 *db_ = nullptr;

    bool exec(const char *sql) {
        char *err = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL 错误: %s\n", err);
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    bool initTables() {
        return exec(
            "CREATE TABLE IF NOT EXISTS devices ("
            "  device_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE,"
            "  type TEXT NOT NULL,"
            "  location TEXT,"
            "  status TEXT DEFAULT 'offline',"
            "  created_at TEXT DEFAULT (datetime('now','localtime'))"
            ");"
        ) && exec(
            "CREATE TABLE IF NOT EXISTS sensor_data ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  device_id INTEGER NOT NULL,"
            "  temperature REAL,"
            "  humidity REAL,"
            "  timestamp TEXT DEFAULT (datetime('now','localtime')),"
            "  FOREIGN KEY(device_id) REFERENCES devices(device_id)"
            ");"
        ) && exec(
            "CREATE TABLE IF NOT EXISTS alert_rules ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  device_id INTEGER NOT NULL,"
            "  metric TEXT NOT NULL,"
            "  condition TEXT NOT NULL,"
            "  threshold REAL NOT NULL,"
            "  enabled INTEGER DEFAULT 1,"
            "  FOREIGN KEY(device_id) REFERENCES devices(device_id)"
            ");"
        ) && exec(
            "CREATE TABLE IF NOT EXISTS alert_events ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  rule_id INTEGER NOT NULL,"
            "  device_id INTEGER NOT NULL,"
            "  value REAL,"
            "  timestamp TEXT DEFAULT (datetime('now','localtime')),"
            "  FOREIGN KEY(rule_id) REFERENCES alert_rules(id),"
            "  FOREIGN KEY(device_id) REFERENCES devices(device_id)"
            ");"
        ) && exec(
            "CREATE INDEX IF NOT EXISTS idx_sensor_device ON sensor_data(device_id);"
        ) && exec(
            "CREATE INDEX IF NOT EXISTS idx_sensor_time ON sensor_data(timestamp);"
        );
    }

    void recordAlert(int rule_id, int device_id, double value) {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO alert_events (rule_id, device_id, value) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, rule_id);
        sqlite3_bind_int(stmt, 2, device_id);
        sqlite3_bind_double(stmt, 3, value);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
};

// ==========================================
// 主程序：模拟嵌入式数据采集系统
// ==========================================
int main() {
    printf("========================================\n");
    printf("  嵌入式传感器数据管理系统 Demo\n");
    printf("========================================\n");

    SensorDatabase sdb;
    if (!sdb.open("embedded_demo.db")) {
        return 1;
    }
    printf("[系统] 数据库初始化完成\n");

    // 1. 注册设备
    printf("\n--- 1. 注册设备 ---\n");
    int dev1 = sdb.addDevice("室内温湿度计", "DHT22", "机房A");
    int dev2 = sdb.addDevice("室外温湿度计", "SHT31", "楼顶");
    int dev3 = sdb.addDevice("车间温湿度计", "DHT22", "车间B");
    printf("  注册设备: ID=%d, %d, %d\n", dev1, dev2, dev3);

    sdb.setDeviceStatus(dev1, "online");
    sdb.setDeviceStatus(dev2, "online");
    sdb.setDeviceStatus(dev3, "online");

    // 2. 设置告警规则
    printf("\n--- 2. 设置告警规则 ---\n");
    sdb.addAlertRule(dev1, "temperature", ">", 30.0);
    sdb.addAlertRule(dev1, "humidity", ">", 80.0);
    sdb.addAlertRule(dev2, "temperature", "<", -10.0);
    sdb.addAlertRule(dev3, "temperature", ">", 35.0);
    printf("  已设置 4 条告警规则\n");

    // 3. 模拟数据采集（批量）
    printf("\n--- 3. 模拟数据采集 ---\n");
    const int BATCH = 100;
    double temps[BATCH], humids[BATCH];

    // 设备1：室内，温度 22-32°C
    for (int i = 0; i < BATCH; i++) {
        temps[i] = 25.0 + 5.0 * sin(i * 0.1);
        humids[i] = 60.0 + 10.0 * cos(i * 0.08);
    }
    int n1 = sdb.insertBatch(dev1, temps, humids, BATCH);

    // 设备2：室外，温度 -5 到 15°C
    for (int i = 0; i < BATCH; i++) {
        temps[i] = 5.0 + 10.0 * sin(i * 0.05);
        humids[i] = 70.0 + 15.0 * cos(i * 0.07);
    }
    int n2 = sdb.insertBatch(dev2, temps, humids, BATCH);

    // 设备3：车间，温度 28-40°C
    for (int i = 0; i < BATCH; i++) {
        temps[i] = 33.0 + 5.0 * sin(i * 0.12);
        humids[i] = 45.0 + 8.0 * cos(i * 0.09);
    }
    int n3 = sdb.insertBatch(dev3, temps, humids, BATCH);
    printf("  设备1插入 %d 条, 设备2插入 %d 条, 设备3插入 %d 条\n", n1, n2, n3);

    // 4. 检查告警
    printf("\n--- 4. 检查告警 ---\n");
    int total_alerts = 0;
    // 对最后一批数据检查告警
    for (int i = 0; i < BATCH; i++) {
        double t1 = 25.0 + 5.0 * sin(i * 0.1);
        double h1 = 60.0 + 10.0 * cos(i * 0.08);
        total_alerts += sdb.checkAndRecordAlerts(dev1, t1, h1);

        double t3 = 33.0 + 5.0 * sin(i * 0.12);
        double h3 = 45.0 + 8.0 * cos(i * 0.09);
        total_alerts += sdb.checkAndRecordAlerts(dev3, t3, h3);
    }
    printf("  共触发 %d 条告警\n", total_alerts);

    // 5. 查看设备概览
    sdb.printDeviceSummary();

    // 6. 查看告警记录
    sdb.printAlertEvents();

    // 7. 系统维护
    printf("\n--- 7. 系统维护 ---\n");
    bool ok = sdb.integrityCheck();
    printf("  完整性检查: %s\n", ok ? "通过 ✓" : "失败 ✗");

    int cleaned = sdb.cleanOldData(30);
    printf("  清理 30 天前的数据: %d 条\n", cleaned);

    // 8. 设备离线
    printf("\n--- 8. 设备状态管理 ---\n");
    sdb.setDeviceStatus(dev3, "offline");
    printf("  设备 %d 已设为离线\n", dev3);

    sdb.close();

    // 清理测试文件
    remove("embedded_demo.db");
    remove("embedded_demo.db-wal");
    remove("embedded_demo.db-shm");

    printf("\n========================================\n");
    printf("  Demo 完成\n");
    printf("========================================\n");
    return 0;
}
