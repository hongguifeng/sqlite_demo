#include <sqlite3.h>
#include <cstdio>
#include <chrono>

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

static int count_rows(sqlite3 *db, const char *table) {
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", table);
    sqlite3_stmt *stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("transaction_demo.db", &db);

    exec_sql(db,
        "CREATE TABLE sensor_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id INTEGER, temperature REAL, humidity REAL);");

    // ==========================================
    // 1. 性能对比：无事务 vs 有事务
    // ==========================================
    printf("=== 1. 事务性能对比 ===\n");

    const int N = 5000;

    // 1a. 无事务（每条自动提交）
    auto t1 = std::chrono::steady_clock::now();
    {
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        for (int i = 0; i < N; i++) {
            sqlite3_bind_int(stmt, 1, i % 10 + 1);
            sqlite3_bind_double(stmt, 2, 20.0 + (i % 100) * 0.1);
            sqlite3_bind_double(stmt, 3, 50.0 + (i % 50) * 0.2);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
        sqlite3_finalize(stmt);
    }
    auto t2 = std::chrono::steady_clock::now();
    auto dur_no_tx = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("  无事务插入 %d 条: %lld ms\n", N, (long long)dur_no_tx);

    // 清空表
    exec_sql(db, "DELETE FROM sensor_data;");

    // 1b. 使用事务
    t1 = std::chrono::steady_clock::now();
    {
        exec_sql(db, "BEGIN TRANSACTION;");
        sqlite3_stmt *stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
            -1, &stmt, nullptr);
        for (int i = 0; i < N; i++) {
            sqlite3_bind_int(stmt, 1, i % 10 + 1);
            sqlite3_bind_double(stmt, 2, 20.0 + (i % 100) * 0.1);
            sqlite3_bind_double(stmt, 3, 50.0 + (i % 50) * 0.2);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
        sqlite3_finalize(stmt);
        exec_sql(db, "COMMIT;");
    }
    t2 = std::chrono::steady_clock::now();
    auto dur_tx = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("  有事务插入 %d 条: %lld ms\n", N, (long long)dur_tx);
    if (dur_tx > 0) {
        printf("  性能提升: 约 %lld 倍\n", (long long)(dur_no_tx / dur_tx));
    }

    exec_sql(db, "DELETE FROM sensor_data;");

    // ==========================================
    // 2. 事务回滚演示
    // ==========================================
    printf("\n=== 2. 事务回滚演示 ===\n");

    exec_sql(db, "BEGIN TRANSACTION;");
    exec_sql(db, "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (1, 25.0, 60.0);");
    exec_sql(db, "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (2, 26.0, 58.0);");
    printf("  事务内插入了 2 条记录\n");
    printf("  COMMIT 前记录数: %d\n", count_rows(db, "sensor_data"));

    // 回滚！
    exec_sql(db, "ROLLBACK;");
    printf("  ROLLBACK 后记录数: %d (数据已撤销)\n", count_rows(db, "sensor_data"));

    // ==========================================
    // 3. SAVEPOINT 演示（部分回滚）
    // ==========================================
    printf("\n=== 3. SAVEPOINT 部分回滚 ===\n");

    exec_sql(db, "BEGIN TRANSACTION;");
    exec_sql(db, "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (1, 25.0, 60.0);");
    printf("  插入第 1 条 → 当前记录数: %d\n", count_rows(db, "sensor_data"));

    exec_sql(db, "SAVEPOINT sp1;");  // 创建保存点
    exec_sql(db, "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (2, 99.9, 99.9);");
    printf("  插入第 2 条(异常数据) → 当前记录数: %d\n", count_rows(db, "sensor_data"));

    // 发现数据异常，回滚到保存点
    exec_sql(db, "ROLLBACK TO sp1;");
    printf("  ROLLBACK TO sp1 → 当前记录数: %d (异常数据已撤销)\n", count_rows(db, "sensor_data"));

    exec_sql(db, "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (2, 24.0, 62.0);");
    printf("  插入第 2 条(正常数据) → 当前记录数: %d\n", count_rows(db, "sensor_data"));

    exec_sql(db, "RELEASE sp1;");  // 释放保存点
    exec_sql(db, "COMMIT;");
    printf("  COMMIT 后记录数: %d\n", count_rows(db, "sensor_data"));

    // ==========================================
    // 4. WAL 模式
    // ==========================================
    printf("\n=== 4. WAL 模式 ===\n");

    // 查看当前日志模式
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, "PRAGMA journal_mode;", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  当前日志模式: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // 切换到 WAL 模式
    sqlite3_prepare_v2(db, "PRAGMA journal_mode = WAL;", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  切换后日志模式: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // WAL 模式下的插入性能
    exec_sql(db, "DELETE FROM sensor_data;");
    t1 = std::chrono::steady_clock::now();
    {
        exec_sql(db, "BEGIN TRANSACTION;");
        sqlite3_stmt *ins_stmt = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
            -1, &ins_stmt, nullptr);
        for (int i = 0; i < N; i++) {
            sqlite3_bind_int(ins_stmt, 1, i % 10 + 1);
            sqlite3_bind_double(ins_stmt, 2, 20.0 + (i % 100) * 0.1);
            sqlite3_bind_double(ins_stmt, 3, 50.0 + (i % 50) * 0.2);
            sqlite3_step(ins_stmt);
            sqlite3_reset(ins_stmt);
            sqlite3_clear_bindings(ins_stmt);
        }
        sqlite3_finalize(ins_stmt);
        exec_sql(db, "COMMIT;");
    }
    t2 = std::chrono::steady_clock::now();
    auto dur_wal = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    printf("  WAL 模式事务插入 %d 条: %lld ms\n", N, (long long)dur_wal);

    sqlite3_close(db);
    remove("transaction_demo.db");
    remove("transaction_demo.db-wal");
    remove("transaction_demo.db-shm");
    printf("\n示例完成。\n");
    return 0;
}
