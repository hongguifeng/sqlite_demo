#include <sqlite3.h>
#include <cstdio>

int main() {
    sqlite3 *db = nullptr;

    // 打开（或创建）数据库文件
    int rc = sqlite3_open("test.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("成功打开数据库！\n");
    printf("SQLite 版本: %s\n", sqlite3_libversion());

    // 使用预编译语句查询版本
    sqlite3_stmt *stmt = nullptr;
    rc = sqlite3_prepare_v2(db, "SELECT sqlite_version();", -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        printf("SQL 查询返回的版本: %s\n", sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    // 关闭数据库
    sqlite3_close(db);
    printf("数据库已关闭。\n");

    // 清理测试文件
    remove("test.db");

    return 0;
}
