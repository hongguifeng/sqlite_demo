#include <sqlite3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// ==========================================
// 自定义 SQL 函数：计算两点间的距离
// 用法: SELECT distance(x1, y1, x2, y2);
// ==========================================
static void distance_func(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    if (argc != 4) {
        sqlite3_result_error(ctx, "distance() requires 4 arguments", -1);
        return;
    }
    double x1 = sqlite3_value_double(argv[0]);
    double y1 = sqlite3_value_double(argv[1]);
    double x2 = sqlite3_value_double(argv[2]);
    double y2 = sqlite3_value_double(argv[3]);
    double dist = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    sqlite3_result_double(ctx, dist);
}

// ==========================================
// 自定义聚合函数：计算标准差
// ==========================================
struct StdDevCtx {
    double sum;
    double sum_sq;
    int count;
};

static void stddev_step(sqlite3_context *ctx, int, sqlite3_value **argv) {
    auto *p = (StdDevCtx*)sqlite3_aggregate_context(ctx, sizeof(StdDevCtx));
    if (!p) return;
    double val = sqlite3_value_double(argv[0]);
    p->sum += val;
    p->sum_sq += val * val;
    p->count++;
}

static void stddev_final(sqlite3_context *ctx) {
    auto *p = (StdDevCtx*)sqlite3_aggregate_context(ctx, 0);
    if (!p || p->count < 2) {
        sqlite3_result_null(ctx);
        return;
    }
    double mean = p->sum / p->count;
    double variance = (p->sum_sq / p->count) - (mean * mean);
    sqlite3_result_double(ctx, sqrt(variance));
}

int main() {
    sqlite3 *db = nullptr;

    // ==========================================
    // 1. sqlite3_open_v2 高级打开选项
    // ==========================================
    printf("=== 1. 高级打开选项 ===\n");

    int rc = sqlite3_open_v2("api_demo.db", &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    printf("  数据库已打开 (串行模式)\n");

    // 设置忙等超时
    sqlite3_busy_timeout(db, 5000);
    printf("  忙等超时: 5000ms\n");

    // 启用外键约束（SQLite 默认关闭）
    exec_sql(db, "PRAGMA foreign_keys = ON;");
    printf("  外键约束: 已启用\n");

    // ==========================================
    // 2. BLOB 数据存储
    // ==========================================
    printf("\n=== 2. BLOB 数据存储 ===\n");

    exec_sql(db,
        "CREATE TABLE firmware ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  version TEXT NOT NULL,"
        "  data BLOB NOT NULL,"
        "  size INTEGER NOT NULL"
        ");");

    // 模拟固件数据（256字节）
    unsigned char firmware_data[256];
    for (int i = 0; i < 256; i++) {
        firmware_data[i] = (unsigned char)(i & 0xFF);
    }

    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db,
        "INSERT INTO firmware (version, data, size) VALUES (?, ?, ?);",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, "v1.0.3", -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, firmware_data, sizeof(firmware_data), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, sizeof(firmware_data));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf("  已存储 %zu 字节的固件数据\n", sizeof(firmware_data));

    // 读取 BLOB 数据
    sqlite3_prepare_v2(db,
        "SELECT version, data, size FROM firmware WHERE id = 1;",
        -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *version = (const char*)sqlite3_column_text(stmt, 0);
        const void *blob = sqlite3_column_blob(stmt, 1);
        int blob_size = sqlite3_column_bytes(stmt, 1);
        int stored_size = sqlite3_column_int(stmt, 2);

        printf("  读取: version=%s, blob_size=%d, stored_size=%d\n",
               version, blob_size, stored_size);

        // 校验数据完整性
        bool match = (blob_size == (int)sizeof(firmware_data)) &&
                     (memcmp(blob, firmware_data, blob_size) == 0);
        printf("  数据完整性校验: %s\n", match ? "通过 ✓" : "失败 ✗");
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 3. 自定义标量函数
    // ==========================================
    printf("\n=== 3. 自定义标量函数: distance() ===\n");

    sqlite3_create_function(db, "distance", 4, SQLITE_UTF8, nullptr,
                           distance_func, nullptr, nullptr);

    exec_sql(db,
        "CREATE TABLE locations ("
        "  id INTEGER PRIMARY KEY, name TEXT, x REAL, y REAL);");
    exec_sql(db,
        "INSERT INTO locations VALUES "
        "(1, '基站A', 0.0, 0.0),"
        "(2, '基站B', 3.0, 4.0),"
        "(3, '基站C', 6.0, 8.0);");

    sqlite3_prepare_v2(db,
        "SELECT a.name, b.name, "
        "ROUND(distance(a.x, a.y, b.x, b.y), 2) as dist "
        "FROM locations a, locations b "
        "WHERE a.id < b.id;", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  %s ↔ %s : %.2f\n",
            sqlite3_column_text(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_double(stmt, 2));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 4. 自定义聚合函数：标准差
    // ==========================================
    printf("\n=== 4. 自定义聚合函数: stddev() ===\n");

    sqlite3_create_function(db, "stddev", 1, SQLITE_UTF8, nullptr,
                           nullptr, stddev_step, stddev_final);

    exec_sql(db,
        "CREATE TABLE measurements (id INTEGER PRIMARY KEY, value REAL);");
    exec_sql(db,
        "INSERT INTO measurements (value) VALUES "
        "(10.0),(12.0),(14.0),(11.0),(13.0),(15.0),(9.0),(16.0);");

    sqlite3_prepare_v2(db,
        "SELECT COUNT(*) as n, ROUND(AVG(value), 2) as avg, "
        "ROUND(stddev(value), 2) as sd FROM measurements;",
        -1, &stmt, nullptr);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  样本数=%d  平均值=%s  标准差=%s\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_text(stmt, 1),
            sqlite3_column_text(stmt, 2));
    }
    sqlite3_finalize(stmt);

    // ==========================================
    // 5. 内存数据库
    // ==========================================
    printf("\n=== 5. 内存数据库 ===\n");

    sqlite3 *mem_db = nullptr;
    sqlite3_open(":memory:", &mem_db);
    printf("  内存数据库已打开\n");

    exec_sql(mem_db, "CREATE TABLE temp_data (id INTEGER, value REAL);");
    exec_sql(mem_db, "INSERT INTO temp_data VALUES (1, 3.14), (2, 2.72);");

    sqlite3_prepare_v2(mem_db, "SELECT * FROM temp_data;", -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("  id=%d  value=%.2f\n",
            sqlite3_column_int(stmt, 0),
            sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(mem_db);
    printf("  内存数据库已关闭（数据自动销毁）\n");

    // ==========================================
    // 6. 错误处理详解
    // ==========================================
    printf("\n=== 6. 错误处理详解 ===\n");

    // 故意触发错误
    rc = sqlite3_prepare_v2(db, "SELECT * FROM nonexistent_table;", -1, &stmt, nullptr);
    printf("  错误码: %d (%s)\n", rc, sqlite3_errstr(rc));
    printf("  错误信息: %s\n", sqlite3_errmsg(db));
    printf("  扩展错误码: %d\n", sqlite3_extended_errcode(db));

    sqlite3_close(db);
    remove("api_demo.db");
    printf("\n示例完成。\n");
    return 0;
}
