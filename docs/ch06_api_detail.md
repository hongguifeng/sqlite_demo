# 第六章：SQLite C/C++ API 详解

本章深入讲解 SQLite 常用 API，帮助你在实际项目中灵活运用。

## 目录

1. [6.1 API 分层结构](#61-api-分层结构)
2. [6.2 数据库打开选项](#62-数据库打开选项)
3. [6.3 BLOB 数据处理](#63-blob-数据处理)
4. [6.4 自定义函数](#64-自定义函数)
5. [6.5 忙等处理](#65-忙等处理)
6. [6.6 错误处理与资源管理](#66-错误处理与资源管理)
7. [6.7 示例代码讲解](#67-示例代码讲解)
8. [6.8 本章小结](#68-本章小结)

### 本章会学到什么

学完这一章后，你应该能回答下面这些问题：

1. `sqlite3_exec()` 和 `prepare/bind/step/finalize` 这套核心 API 应该怎么分工。
2. `sqlite3_open_v2()` 比 `sqlite3_open()` 多了哪些控制能力。
3. 如何在 SQLite 中安全读写 BLOB 二进制数据。
4. 如何把 C/C++ 函数注册成可在 SQL 中直接调用的自定义函数。
5. 如何设置忙等超时、如何读取错误码和错误信息。
6. 为什么资源释放和错误检查在 SQLite C API 中非常重要。

## 6.1 API 分层结构

```
┌────────────────────────────────────────────┐
│           应用层（你的代码）                   │
├────────────────────────────────────────────┤
│  高层 API                                   │
│  sqlite3_exec()    ← 简单执行，适合 DDL      │
├────────────────────────────────────────────┤
│  核心 API（推荐使用）                         │
│  sqlite3_prepare_v2()  → 编译 SQL            │
│  sqlite3_bind_*()      → 绑定参数            │
│  sqlite3_step()        → 执行/取下一行        │
│  sqlite3_column_*()    → 读取列值            │
│  sqlite3_finalize()    → 释放语句            │
│  sqlite3_reset()       → 重置语句（复用）     │
├────────────────────────────────────────────┤
│  数据库管理 API                              │
│  sqlite3_open_v2()     → 打开（高级选项）     │
│  sqlite3_close_v2()    → 关闭               │
│  sqlite3_errmsg()      → 错误信息            │
│  sqlite3_errcode()     → 错误码              │
├────────────────────────────────────────────┤
│  扩展 API                                   │
│  sqlite3_create_function()  → 自定义函数     │
│  sqlite3_busy_handler()     → 忙等处理       │
│  sqlite3_backup_*()         → 在线备份       │
└────────────────────────────────────────────┘
```

这张图最重要的意义，不是让你死记全部函数名，而是建立一个清晰的使用层次感。

### 第一层：最简接口 `sqlite3_exec()`

它适合：

1. 一次性执行简单 SQL；
2. 不需要参数绑定；
3. 不需要逐行读取结果；
4. 做快速初始化或配置。

典型场景：

```cpp
sqlite3_exec(db, "CREATE TABLE ...;", nullptr, nullptr, &err_msg);
sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &err_msg);
```

因此可以把它理解成：

> “给 SQLite 一段 SQL 字符串，让它直接执行。”

### 第二层：核心执行链 `prepare -> bind -> step -> column -> finalize`

这是 SQLite C API 中最重要的一层，也是工程上最常用的一层。

它适合：

1. 带参数的 SQL；
2. 需要重复执行的 SQL；
3. 需要逐行读取结果的查询；
4. 需要精细错误处理和资源管理的场景。

你可以把这条链记成：

1. `sqlite3_prepare_v2()`：把 SQL 编译成语句对象；
2. `sqlite3_bind_*()`：填入本次参数；
3. `sqlite3_step()`：执行或取下一行；
4. `sqlite3_column_*()`：读取当前结果行列值；
5. `sqlite3_finalize()`：释放语句对象。

### 第三层：数据库管理与扩展能力

这部分 API 不是每条 SQL 都会用到，但它决定了程序是否足够健壮和灵活：

1. `sqlite3_open_v2()`：控制数据库如何打开；
2. `sqlite3_errmsg()` / `sqlite3_errcode()`：定位失败原因；
3. `sqlite3_create_function()`：扩展 SQL 能力；
4. `sqlite3_busy_timeout()`：应对锁竞争；
5. `sqlite3_backup_*()`：做在线备份。

因此，第六章真正要建立的是这样一个心智模型：

> SQLite 不只是“执行 SQL 的黑盒”，它还给了你一套完整的数据库控制接口。

## 6.2 数据库打开选项

`sqlite3_open_v2()` 提供比 `sqlite3_open()` 更多的控制：

```cpp
int sqlite3_open_v2(
    const char *filename,   // 数据库文件名
    sqlite3 **ppDb,         // 输出：数据库句柄
    int flags,              // 打开标志
    const char *zVfs        // VFS 名称（通常为 NULL）
);
```

常用标志：

| 标志 | 含义 |
|------|------|
| `SQLITE_OPEN_READONLY` | 只读打开 |
| `SQLITE_OPEN_READWRITE` | 读写打开 |
| `SQLITE_OPEN_CREATE` | 不存在时创建 |
| `SQLITE_OPEN_MEMORY` | 内存数据库 |
| `SQLITE_OPEN_NOMUTEX` | 多线程模式（无互斥锁） |
| `SQLITE_OPEN_FULLMUTEX` | 串行模式（完全互斥） |

### `sqlite3_open_v2()` 和 `sqlite3_open()` 的区别

`sqlite3_open()` 用起来更简单，但控制能力有限；`sqlite3_open_v2()` 则允许你明确指定数据库以什么模式打开。

这意味着你可以直接告诉 SQLite：

1. 只读还是读写；
2. 文件不存在时是否自动创建；
3. 是否使用内存数据库；
4. 连接的线程互斥模式是什么。

因此它更适合工程场景，而不只是最简示例。

### 示例代码中的打开方式在表达什么

第六章示例里使用的是：

```cpp
sqlite3_open_v2("api_demo.db", &db,
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
    nullptr);
```

这可以拆成三部分：

1. `SQLITE_OPEN_READWRITE`：允许读写；
2. `SQLITE_OPEN_CREATE`：如果文件不存在就创建；
3. `SQLITE_OPEN_FULLMUTEX`：使用完全互斥的串行模式。

合起来的含义是：

> 打开一个可读写、必要时可自动创建、并且线程安全策略更保守的数据库连接。

### `SQLITE_OPEN_FULLMUTEX` 应该怎么理解

这是很多初学者容易忽略的点。它不是 SQL 层面的功能，而是连接层面的线程互斥策略。

可以先把它理解为：

1. 更偏向安全的串行模式；
2. 适合你不想自己承担太多并发细节时使用；
3. 代价通常是比无互斥模式多一些同步开销。

对入门和教学代码来说，这种保守选项通常是合理的。

### 内存数据库是什么时候有用

文档表格里列出了 `SQLITE_OPEN_MEMORY`，示例代码也演示了 `:memory:` 数据库。

内存数据库的特点是：

1. 数据完全驻留在内存中；
2. 进程结束或连接关闭后数据消失；
3. 适合临时计算、单元测试、缓存型场景、演示代码。

这在嵌入式和工具程序里都很常见。

## 6.3 BLOB 数据处理

嵌入式系统经常需要存储二进制数据（固件片段、图像、波形数据等）：

```cpp
// 写入 BLOB
sqlite3_bind_blob(stmt, col, data_ptr, data_size, SQLITE_TRANSIENT);

// 读取 BLOB
const void *blob = sqlite3_column_blob(stmt, col);
int blob_size = sqlite3_column_bytes(stmt, col);
```

### 什么是 BLOB

`BLOB` 是 Binary Large Object 的缩写，可以把它简单理解成“原样保存的一段二进制字节流”。

和 `TEXT` 不同，BLOB 不假设内容是可打印字符串；它可以是：

1. 固件数据；
2. 传感器原始采样帧；
3. 图像片段；
4. 音频片段；
5. 自定义协议包。

### 为什么不能把二进制数据当字符串处理

二进制数据中可能包含：

1. `0x00` 这样的空字节；
2. 不可打印字符；
3. 任意字节序列；

如果你把它当 `TEXT` 或 C 字符串处理，就可能：

1. 提前截断；
2. 内容损坏；
3. 长度判断错误。

因此，BLOB 必须配合“指针 + 显式长度”来处理。

### 写入 BLOB 时最关键的两个参数

```cpp
sqlite3_bind_blob(stmt, 2, firmware_data, sizeof(firmware_data), SQLITE_TRANSIENT);
```

这里最重要的是：

1. 数据起始地址 `firmware_data`
2. 数据长度 `sizeof(firmware_data)`

SQLite 不会像处理 C 字符串那样通过结尾的 `\0` 推断长度，所以你必须明确告诉它这段二进制数据有多长。

### 读取 BLOB 的标准方式

```cpp
const void *blob = sqlite3_column_blob(stmt, 1);
int blob_size = sqlite3_column_bytes(stmt, 1);
```

这两句通常要配合使用：

1. `sqlite3_column_blob()` 返回数据指针；
2. `sqlite3_column_bytes()` 返回该列当前结果值的字节长度。

对 BLOB 来说，单拿到指针是不够的，必须同时知道长度。

### 示例里的固件数据演示了什么

示例代码先构造了一个 256 字节的数组，然后：

1. 把它作为 BLOB 插入 `firmware` 表；
2. 再把这一行读出来；
3. 最后用 `memcmp()` 比较原始内容和数据库读出的内容是否完全一致。

这在教学上很重要，因为它不只是在演示“能写进去”，还在演示：

> 二进制数据经过 SQLite 存取之后，是否保持了原始字节级一致性。

对固件、校准数据、波形缓存这类场景来说，这种完整性验证非常关键。

## 6.4 自定义函数

SQLite 允许你用 C/C++ 编写自定义 SQL 函数：

```cpp
sqlite3_create_function(db, "函数名", 参数数量, 编码, 用户数据,
                        标量函数回调, 聚合步骤回调, 聚合结束回调);
```

这是 SQLite 非常强大的能力之一。它意味着：

> 你不仅能用 SQL 调用数据库自带函数，还能把自己写的 C/C++ 逻辑注册成 SQL 函数。

### 为什么自定义函数很有用

当你发现某个业务计算：

1. 经常出现在查询里；
2. 用纯 SQL 写起来不方便；
3. 或者已经有成熟的 C/C++ 实现；

这时就很适合把它封装成 SQLite 自定义函数。

这样做的好处是：

1. 查询表达力更强；
2. 复杂计算可以直接放到 SQL 层；
3. 应用代码和查询逻辑更一致。

### 标量函数和聚合函数的区别

示例代码里刚好演示了两种最重要的自定义函数类型。

#### 1. 标量函数

标量函数对“当前一行输入”进行计算，返回一个结果值。

示例中的：

```cpp
sqlite3_create_function(db, "distance", 4, SQLITE_UTF8, nullptr,
                       distance_func, nullptr, nullptr);
```

把 `distance(x1, y1, x2, y2)` 注册成了一个 SQL 函数。之后你就可以直接在查询里写：

```sql
SELECT distance(a.x, a.y, b.x, b.y)
```

#### 2. 聚合函数

聚合函数不是对单行计算，而是对一组行累积计算，最后输出一个汇总值。

示例中的：

```cpp
sqlite3_create_function(db, "stddev", 1, SQLITE_UTF8, nullptr,
                       nullptr, stddev_step, stddev_final);
```

把 `stddev(value)` 注册成了一个聚合函数，用来计算标准差。

### 标量函数回调是怎么工作的

示例里的 `distance_func()`：

1. 先检查参数个数是否正确；
2. 用 `sqlite3_value_double()` 取出 4 个参数；
3. 在 C++ 中计算欧氏距离；
4. 用 `sqlite3_result_double()` 把结果返回给 SQLite。

这说明自定义函数的基本模式是：

1. 从 SQLite 读取输入参数；
2. 在 C/C++ 中执行业务逻辑；
3. 再把结果写回 SQLite 上下文。

### 聚合函数为什么需要 `step` 和 `final`

聚合函数要处理一组行，因此 SQLite 需要两个阶段：

1. `step`：每来一行，就累计一次状态；
2. `final`：所有行处理完后，输出最终结果。

示例中的标准差实现使用了：

1. `sum`
2. `sum_sq`
3. `count`

来累积数据，然后在 `stddev_final()` 中计算均值、方差和标准差。

### `sqlite3_aggregate_context()` 在做什么

这是聚合函数示例里最值得特别解释的 API。它的作用是：

> 为当前聚合计算提供一块可持续保存的上下文内存。

也就是说：

1. 第一行调用 `step` 时，可以创建并初始化上下文；
2. 后续每一行继续复用这块上下文；
3. 到 `final` 时，再读取这块上下文里的累计结果。

如果没有它，聚合函数就没法跨多行持续累积状态。

## 6.5 忙等处理

当数据库被其他连接锁定时，SQLite 默认立即返回 `SQLITE_BUSY` 错误。你可以设置忙等超时：

```cpp
// 方法1：设置超时时间（毫秒）
sqlite3_busy_timeout(db, 5000);  // 等待最多 5 秒

// 方法2：自定义忙等处理回调
sqlite3_busy_handler(db, busy_callback, user_data);
```

### 什么情况下会触发 `SQLITE_BUSY`

最常见的情况是：

1. 另一个连接正在持有写锁；
2. 当前连接也想写，或无法立即获得所需锁；
3. SQLite 于是返回“数据库忙”。

它不是语法错误，也不是数据库损坏，而是并发竞争下的访问冲突。

### `sqlite3_busy_timeout()` 是最简单的应对方式

示例代码里调用了：

```cpp
sqlite3_busy_timeout(db, 5000);
```

它的含义是：

> 如果数据库当前忙，不要立刻失败，最多等待 5000 毫秒。

这对很多轻量级应用已经足够实用，因为它能避免瞬时锁竞争导致的立刻报错。

### `sqlite3_busy_handler()` 什么时候更适合

如果你想自己决定：

1. 重试多少次；
2. 何时放弃；
3. 是否打印日志；
4. 是否结合业务上下文做特殊处理；

那么就可以使用 `sqlite3_busy_handler()` 注册自定义回调。

入门阶段先掌握 `sqlite3_busy_timeout()` 就足够，等需要更复杂策略时再升级到自定义回调。

## 6.6 错误处理与资源管理

SQLite C API 的一个核心特点是：

> 它非常灵活，但这也意味着调用者必须自己承担更多错误检查和资源释放责任。

### 最常见的错误信息 API

#### `sqlite3_errmsg(db)`

返回当前数据库连接最近一次错误的人类可读文本。

例如：

```cpp
fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
```

#### `sqlite3_errcode(db)`

返回当前连接最近一次基础错误码。

#### `sqlite3_extended_errcode(db)`

返回更细粒度的扩展错误码，适合做更精确的问题定位。

#### `sqlite3_errstr(rc)`

根据返回码 `rc` 给出对应的错误说明字符串。

### 为什么要同时看“返回码”和“错误信息”

只看 `errmsg`，你知道发生了什么；只看错误码，你知道错误属于哪一类。但两者结合起来，问题定位会更准确。

因此工程上很推荐的模式是：

1. 先检查函数返回码；
2. 若失败，再打印或记录 `sqlite3_errmsg()`；
3. 必要时进一步查看扩展错误码。

### 示例中的错误处理演示了什么

示例故意执行：

```sql
SELECT * FROM nonexistent_table;
```

然后打印：

1. 错误码 `rc`
2. `sqlite3_errstr(rc)`
3. `sqlite3_errmsg(db)`
4. `sqlite3_extended_errcode(db)`

这是一种非常好的调试套路，因为它同时展示了：

1. API 返回值层面的错误；
2. 连接对象上的详细错误状态；
3. 可读文本和可编程错误码两种视角。

### 为什么 `sqlite3_finalize()` 和 `sqlite3_close()` 一定要认真做

SQLite 的语句对象和数据库连接都不是自动垃圾回收的资源。

因此：

1. `sqlite3_finalize(stmt)` 负责释放语句对象；
2. `sqlite3_close(db)` 或 `sqlite3_close_v2(db)` 负责关闭数据库连接；

如果你忘记释放，就可能导致：

1. 资源泄漏；
2. 连接无法正常关闭；
3. 长时间运行程序积累问题。

所以，SQLite C API 的一个核心工程习惯是：

> 谁创建资源，谁负责在合适时机释放资源。

## 6.7 示例代码讲解

完整示例：[examples/ch06_api_detail.cpp](../examples/ch06_api_detail.cpp)

运行方式：

```bash
cd build && ./ch06_api_detail
```

### 代码结构总览

这个示例程序分成 6 个主题：

1. 高级打开选项与忙等超时；
2. BLOB 数据存取；
3. 自定义标量函数；
4. 自定义聚合函数；
5. 内存数据库；
6. 错误处理演示。

### 1. 打开数据库后，为什么立刻设置超时和外键

示例程序一打开数据库，就做了两件额外的事：

1. `sqlite3_busy_timeout(db, 5000);`
2. `PRAGMA foreign_keys = ON;`

这体现了一个很实际的工程习惯：数据库连接建立后，通常马上就要完成一些连接级初始化。

例如：

1. 设置忙等策略；
2. 打开外键约束；
3. 设置 journal mode；
4. 设置 synchronous 级别；

因此，打开连接往往只是开始，不是完整初始化的终点。

### 2. BLOB 示例为什么还额外保存了 `size` 列

示例里的 `firmware` 表同时保存：

1. `data BLOB`
2. `size INTEGER`

从纯技术上说，BLOB 本身已经能通过 `sqlite3_column_bytes()` 获取长度；但额外存一列 `size`，在教学和工程上都很常见，因为它能：

1. 让数据结构更自描述；
2. 方便 SQL 层直接查看长度；
3. 用于额外的一致性校验。

### 3. `distance()` 示例为什么适合当第一个自定义函数

因为它非常直观：

1. 输入是 4 个数值参数；
2. 输出是 1 个数值结果；
3. 不依赖复杂状态；
4. 很容易验证结果是否正确。

它几乎是“如何把 C++ 计算逻辑挂进 SQL”最清晰的入门例子之一。

### 4. `stddev()` 示例为什么更接近真实扩展开发

相比 `distance()`，标准差聚合函数更像真正的数据库扩展能力，因为它体现了：

1. 按行累积状态；
2. 在最终阶段输出结果；
3. 使用自定义上下文结构体保存中间数据。

一旦你理解了这个模式，后面就能类比实现更多聚合逻辑，例如：

1. 自定义平均绝对偏差；
2. 自定义加权平均；
3. 某些业务特有统计量。

### 5. 内存数据库演示的是 SQLite 的“嵌入式性”

```cpp
sqlite3_open(":memory:", &mem_db);
```

这行代码非常能体现 SQLite 的特点：你不需要数据库服务进程，就能在程序内部直接拥有一个临时数据库。

这适合：

1. 测试；
2. 临时计算；
3. 小型缓存；
4. 演示代码。

而示例里“关闭后数据自动销毁”这句话，正好把它和普通磁盘数据库区分开来了。

### 6. 这一章和前几章最大的不同是什么

前三章、四五章更偏向 SQL 语法和数据库行为；第六章开始明显转向“程序员如何控制 SQLite 本身”。

也就是说，本章关注的不只是：

1. SQL 写什么；
2. 连接怎么打开；
3. 错误怎么处理；
4. 数据类型怎么读写；
5. SQL 能力怎么扩展；
6. 资源怎么安全释放。

因此第六章是从“会用 SQLite”走向“会在工程代码里正确集成 SQLite”的关键一章。

## 6.8 本章小结

- `sqlite3_open_v2()` 提供更精细的数据库打开控制
- BLOB 处理让你可以存储任意二进制数据
- 自定义函数扩展了 SQL 的能力
- `sqlite3_busy_timeout()` 处理并发锁冲突
- 正确的错误处理和资源管理是编写健壮代码的关键

### 把本章浓缩成一套心智模型

如果要把第六章压缩成最核心的几句话，可以记成：

1. `sqlite3_exec()` 适合简单直接执行；
2. `prepare/bind/step/column/finalize` 是最核心的工程执行链；
3. `open_v2`、错误处理、忙等策略决定程序是否足够健壮；
4. BLOB 和自定义函数让 SQLite 能更好地适配嵌入式和业务扩展场景；
5. SQLite C API 很灵活，因此调用者必须认真做错误检查和资源释放。

### 从本章进入下一章前，你应该已经掌握什么

如果这一章已经掌握，你应该能够：

1. 根据场景选择 `sqlite3_exec()` 还是核心执行链接口。
2. 使用 `sqlite3_open_v2()` 打开合适模式的数据库连接。
3. 正确读写 BLOB 数据并验证完整性。
4. 注册并使用简单的自定义标量函数和聚合函数。
5. 通过错误码、错误信息和资源释放写出更健壮的 SQLite 代码。

---

[⬅ 第五章：事务与并发控制](ch05_transaction.md) | [目录](../README.md) | [第七章：性能优化 ➡](ch07_performance.md)
