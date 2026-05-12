cmake_minimum_required(VERSION 3.14)
project(sqlite_tutorial LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找系统安装的 SQLite3
# 需要安装: sudo apt install libsqlite3-dev
find_package(SQLite3 REQUIRED)
message(STATUS "Found SQLite3: ${SQLite3_VERSION}")

# 定义一个辅助函数来添加示例
function(add_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE SQLite::SQLite3)
endfunction()

# 各章节示例程序
add_example(ch02_hello_sqlite   examples/ch02_hello_sqlite.cpp)
add_example(ch03_create_table   examples/ch03_create_table.cpp)
add_example(ch03_insert_data    examples/ch03_insert_data.cpp)
add_example(ch03_query_data     examples/ch03_query_data.cpp)
add_example(ch03_update_delete  examples/ch03_update_delete.cpp)
add_example(ch04_advanced_query examples/ch04_advanced_query.cpp)
add_example(ch05_transaction    examples/ch05_transaction.cpp)
add_example(ch06_api_detail     examples/ch06_api_detail.cpp)
add_example(ch07_performance    examples/ch07_performance.cpp)
add_example(ch08_embedded       examples/ch08_embedded.cpp)
