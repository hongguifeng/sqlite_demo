#include <sqlite3.h>
#include <cstdio>
#include <chrono>
#include <cmath>

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

static int count_rows(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

using Clock = std::chrono::steady_clock;

static long long elapsed_ms(Clock::time_point t1, Clock::time_point t2) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}

int main() {
    sqlite3 *db = nullptr;
    sqlite3_open("perf_demo.db", &db);

    printf("=== SQLite 性能优化演示 ===\n\n");

    // ==========================================
    // 1. PRAGMA 优化设置
    // ==========================================
    printf("--- 1. 应用 PRAGMA 优化 ---\n");
    exec_sql(db, "PRAGMA journal_mode = WAL;");
    exec_sql(db, "PRAGMA synchronous = NORMAL;");
    exec_sql(db, "PRAGMA cache_size = -8000;");  // ~8MB
    exec_sql(db, "PRAGMA temp_store = MEMORY;");

    sqlite3_stmt *stmt = nullptr;
    const char *pragmas[] = {"journal_mode", "synchronous", "cache_size", "temp_store"};
    for (const auto &p : pragmas) {
        char sql[64];
        snprintf(sql, sizeof(sql), "PRAGMA %s;", p);
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  %s = %s\n", p, sqlite3_column_text(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    // 创建表
    exec_sql(db,
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  device_id INTEGER NOT NULL,"
        "  temperature REAL, humidity REAL,"
        "  timestamp TEXT DEFAULT (datetime('now','localtime')));");

    // ==========================================
    // 2. 批量插入性能测试
    // ==========================================
    const int N = 50000;
    printf("\n--- 2. 批量插入 %d 条记录 ---\n", N);

    auto t1 = Clock::now();
    exec_sql(db, "BEGIN TRANSACTION;");
    sqlite3_prepare_v2(db,
        "INSERT INTO sensor_data (device_id, temperature, humidity) VALUES (?, ?, ?);",
        -1, &stmt, nullptr);
    for (int i = 0; i < N; i++) {
        sqlite3_bind_int(stmt, 1, i % 10 + 1);
        sqlite3_bind_double(stmt, 2, 20.0 + sin(i * 0.01) * 10.0);
        sqlite3_bind_double(stmt, 3, 50.0 + cos(i * 0.01) * 15.0);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
    exec_sql(db, "COMMIT;");
    auto t2 = Clock::now();
    long long insert_ms = elapsed_ms(t1, t2);
    printf("  插入耗时: %lld ms  (%.0f 条/秒)\n", insert_ms,
           insert_ms > 0 ? N * 1000.0 / insert_ms : 0);

    // ==========================================
    // 3. 无索引 vs 有索引查询性能
    // ==========================================
    printf("\n--- 3. 索引对查询性能的影响 ---\n");

    // 无索引查询
    t1 = Clock::now();
    for (int i = 0; i < 100; i++) {
        sqlite3_prepare_v2(db,
            "SELECT AVG(temperature), COUNT(*) FROM sensor_data WHERE device_id = ?;",
            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, (i % 10) + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    t2 = Clock::now();
    printf("  无索引: 100 次查询耗时 %lld ms\n", elapsed_ms(t1, t2));

    // 创建索引
    t1 = Clock::now();
    exec_sql(db, "CREATE INDEX idx_device_id ON sensor_data(device_id);");
    t2 = Clock::now();
    printf("  创建索引耗时: %lld ms\n", elapsed_ms(t1, t2));

    // 有索引查询
    t1 = Clock::now();
    for (int i = 0; i < 100; i++) {
        sqlite3_prepare_v2(db,
            "SELECT AVG(temperature), COUNT(*) FROM sensor_data WHERE device_id = ?;",
            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, (i % 10) + 1);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    t2 = Clock::now();
    printf("  有索引: 100 次查询耗时 %lld ms\n", elapsed_ms(t1, t2));

    // ==========================================
    // 4. EXPLAIN QUERY PLAN
    // ==========================================
    printf("\n--- 4. 查看执行计划 ---\n");
    sqlite3_prepare_v2(db,
        "EXPLAIN QUERY PLAN SELECT * FROM sensor_data WHERE device_id = 1 AND temperature > 25;",
        -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  %s\n", sqlite3_column_text(stmt, 3));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 5. 数据库大小与 VACUUM
    // ==========================================
    printf("\n--- 5. VACUUM 演示 ---\n");

    // 查看当前大小
    sqlite3_prepare_v2(db,
        "SELECT page_count * page_size FROM pragma_page_count, pragma_page_size;",
        -1, &stmt, nullptr);
    long long size_before = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        size_before = sqlite3_column_int64(stmt, 0);
        printf("  删除前数据库大小: %lld KB\n", size_before / 1024);
    }
    sqlite3_finalize(stmt);

    // 删除一半数据
    exec_sql(db, "DELETE FROM sensor_data WHERE id % 2 = 0;");
    printf("  已删除 %d 条记录\n", sqlite3_changes(db));

    // 删除后大小（未 VACUUM）
    sqlite3_prepare_v2(db,
        "SELECT page_count * page_size FROM pragma_page_count, pragma_page_size;",
        -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  删除后大小(未VACUUM): %lld KB\n", sqlite3_column_int64(stmt, 0) / 1024);
    }
    sqlite3_finalize(stmt);

    // VACUUM
    t1 = Clock::now();
    exec_sql(db, "VACUUM;");
    t2 = Clock::now();
    printf("  VACUUM 耗时: %lld ms\n", elapsed_ms(t1, t2));

    sqlite3_prepare_v2(db,
        "SELECT page_count * page_size FROM pragma_page_count, pragma_page_size;",
        -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  VACUUM 后大小: %lld KB\n", sqlite3_column_int64(stmt, 0) / 1024);
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 6. 完整性检查
    // ==========================================
    printf("\n--- 6. 数据库完整性检查 ---\n");
    sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  结果: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 7. ANALYZE
    // ==========================================
    printf("\n--- 7. 统计信息更新 ---\n");
    t1 = Clock::now();
    exec_sql(db, "ANALYZE;");
    t2 = Clock::now();
    printf("  ANALYZE 耗时: %lld ms\n", elapsed_ms(t1, t2));

    printf("\n  最终记录数: %d\n",
           count_rows(db, "SELECT COUNT(*) FROM sensor_data;"));

    sqlite3_close(db);
    remove("perf_demo.db");
    remove("perf_demo.db-wal");
    remove("perf_demo.db-shm");
    printf("\n示例完成。\n");
    return 0;
}
