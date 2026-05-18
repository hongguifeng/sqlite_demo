# 第四章：高级查询

本章介绍更强大的 SQL 查询技巧，包括多表关联（JOIN）、子查询、索引和视图。

## 目录

1. [4.1 多表关联（JOIN）](#41-多表关联join)
2. [4.2 子查询](#42-子查询)
3. [4.3 索引（INDEX）](#43-索引index)
4. [4.4 视图（VIEW）](#44-视图view)
5. [4.5 HAVING 与 CASE 表达式](#45-having-与-case-表达式)
6. [4.6 示例代码讲解](#46-示例代码讲解)
7. [4.7 本章小结](#47-本章小结)

### 本章会学到什么

学完这一章后，你应该能理解并写出下面这些查询：

1. 把两张表按关联键拼起来的 `JOIN` 查询。
2. 在查询里再嵌套查询的子查询。
3. 用 `EXPLAIN QUERY PLAN` 判断索引是否生效。
4. 用视图把复杂统计查询封装成可复用的“虚拟表”。
5. 用 `HAVING` 筛选分组结果，用 `CASE` 给数据做分类。

## 4.1 多表关联（JOIN）

在实际项目中，数据通常分散在多个表中。**JOIN** 操作可以将多个表的数据按照关联条件合并在一起。

这一点在设备数据场景里非常常见：

1. `devices` 表保存设备基础信息。
2. `sensor_data` 表保存每次采样的数据。
3. 两张表通过 `device_id` 建立联系。

如果只查 `sensor_data`，你能看到温度和湿度，但不知道这条数据属于哪台设备；如果只查 `devices`，你知道设备名，但看不到传感器读数。`JOIN` 的作用，就是把这些分散在不同表中的信息拼起来。

### JOIN 的类型

```
表A: devices              表B: sensor_data
┌────┬──────────┐        ┌────┬───────┬──────┐
│ id │ name     │        │ id │dev_id │ temp │
├────┼──────────┤        ├────┼───────┼──────┤
│ 1  │ 传感器A  │        │ 1  │ 1     │ 25.3 │
│ 2  │ 传感器B  │        │ 2  │ 1     │ 25.5 │
│ 3  │ 网关     │        │ 3  │ 2     │ 24.1 │
└────┴──────────┘        │ 4  │ 99    │ 30.0 │  ← 孤立数据
                         └────┴───────┴──────┘
```

| JOIN 类型 | 说明 | 结果 |
|-----------|------|------|
| `INNER JOIN` | 只返回两表都匹配的行 | 不包含网关(无数据)和dev_id=99(无设备) |
| `LEFT JOIN` | 返回左表所有行，右表无匹配则为 NULL | 包含网关(数据为NULL) |
| `CROSS JOIN` | 笛卡尔积（每行×每行） | 很少使用 |

> SQLite 不支持 `RIGHT JOIN` 和 `FULL OUTER JOIN`，但可以用 `LEFT JOIN` + `UNION` 模拟。

### 为什么需要 `ON` 条件

`JOIN` 不是简单把两张表横向拼在一起，而是要告诉数据库“按什么规则匹配”。这个规则就写在 `ON` 子句中。

例如：

```sql
SELECT d.name, s.temperature
FROM sensor_data s
INNER JOIN devices d ON s.device_id = d.device_id;
```

这里的 `ON s.device_id = d.device_id` 表示：

1. 从 `sensor_data` 里取一行；
2. 去 `devices` 中找 `device_id` 相同的设备；
3. 找到后把两边的列拼成一行结果。

如果没有这个匹配条件，数据库就不知道该把哪条传感器数据和哪台设备对应起来。

### 表别名为什么很常见

示例中经常写成：

```sql
FROM sensor_data s
INNER JOIN devices d ON s.device_id = d.device_id
```

这里的 `s` 和 `d` 就是表别名。它们的作用主要有两个：

1. 写 SQL 更短，避免反复写完整表名；
2. 当两张表里有同名列时，可以明确写成 `s.device_id`、`d.device_id`，避免歧义。

对多表查询来说，使用别名几乎是标准写法。

### SQL 示例

```sql
-- INNER JOIN：查询每条传感器数据及其所属设备名称
SELECT d.name, s.temperature, s.humidity, s.timestamp
FROM sensor_data s
INNER JOIN devices d ON s.device_id = d.device_id
WHERE s.temperature > 25.0;

-- LEFT JOIN：查询所有设备及其数据条数（含无数据的设备）
SELECT d.name, d.type, COUNT(s.id) as data_count
FROM devices d
LEFT JOIN sensor_data s ON d.device_id = s.device_id
GROUP BY d.device_id;
```

### 深入理解：`INNER JOIN`

示例代码中的第一条查询：

```sql
SELECT d.name, s.temperature, s.humidity
FROM sensor_data s
INNER JOIN devices d ON s.device_id = d.device_id
ORDER BY s.temperature DESC;
```

它可以拆开理解为：

1. 以 `sensor_data` 为主，先取出所有传感器数据；
2. 通过 `s.device_id = d.device_id` 找到所属设备；
3. 只保留能匹配上的行；
4. 最后按温度从高到低排序。

`INNER JOIN` 的核心语义是：

> 只保留两边都能匹配成功的记录。

所以如果有设备没有任何采样数据，它不会出现在结果里；如果有一条采样记录引用了一个不存在的设备，也不会出现在结果里。

### 深入理解：`LEFT JOIN`

示例代码中的第二条查询：

```sql
SELECT d.name, d.type, COUNT(s.id) as data_count,
ROUND(AVG(s.temperature), 1) as avg_temp
FROM devices d
LEFT JOIN sensor_data s ON d.device_id = s.device_id
GROUP BY d.device_id;
```

这里之所以使用 `LEFT JOIN`，是因为我们想要“所有设备都出现”，哪怕它没有数据。

可以这样理解：

1. 先以左表 `devices` 为基准，把每台设备都拿出来；
2. 再尝试去 `sensor_data` 里匹配采样记录；
3. 如果匹配不上，右表相关列就补成 `NULL`；
4. 最后再按设备分组做统计。

这就是 `LEFT JOIN` 和 `INNER JOIN` 最大的区别：

> `LEFT JOIN` 保留左表完整性，`INNER JOIN` 只保留匹配成功的交集。

### `COUNT(s.id)` 为什么比 `COUNT(*)` 更合适

在 `LEFT JOIN` 场景下，这个区别很重要。

如果写成：

```sql
COUNT(*)
```

那么即使某台设备没有任何采样记录，分组后这台设备那一行也会被算作 1 行。

而示例里写的是：

```sql
COUNT(s.id)
```

这表示只统计右表中真正匹配上的 `s.id`，如果右表没匹配上，`s.id` 为 `NULL`，就不会被计数。这样得到的 `data_count` 才是真实的数据条数。

### `GROUP BY` 在 JOIN 统计中的作用

`LEFT JOIN` 只是把设备和采样记录拼到一起，拼接后的结果通常还是“明细行”。要变成“每台设备一行的统计结果”，就需要 `GROUP BY`。

例如：

```sql
GROUP BY d.device_id
```

它的含义是：

1. 把同一个设备的多条采样记录划成一组；
2. 对每一组计算 `COUNT()`、`AVG()` 等聚合结果；
3. 最终每组输出一行。

因此，`JOIN + GROUP BY` 是统计类查询里非常常见的组合。

## 4.2 子查询

子查询是嵌套在另一个查询中的 SELECT 语句。

```sql
-- 查询温度高于平均值的记录
SELECT * FROM sensor_data
WHERE temperature > (SELECT AVG(temperature) FROM sensor_data);

-- 查询有传感器数据的设备
SELECT * FROM devices
WHERE device_id IN (SELECT DISTINCT device_id FROM sensor_data);
```

子查询可以理解成：

> “先用一个查询算出中间结果，再把这个结果交给外层查询继续使用。”

这非常适合表达“用一个条件依赖另一个查询结果”的需求。

### 子查询的两种常见形态

在本章示例里，出现了两种最典型的子查询用法。

#### 1. 标量子查询

示例：

```sql
SELECT device_id, temperature, humidity
FROM sensor_data
WHERE temperature > (SELECT AVG(temperature) FROM sensor_data);
```

这里的内层查询：

```sql
SELECT AVG(temperature) FROM sensor_data
```

只返回一个值，也就是全表平均温度。这种“只返回单个值”的子查询，叫做标量子查询。

外层查询就把这个值当作一个普通常量来比较：

1. 先算出平均温度；
2. 再找出所有高于平均值的记录。

#### 2. 集合子查询

示例：

```sql
SELECT * FROM devices
WHERE device_id IN (SELECT DISTINCT device_id FROM sensor_data);
```

这里的内层查询会返回一组 `device_id`，而不是单个值。因此它和 `IN` 一起使用，表示：

1. 先找出所有出现在 `sensor_data` 中的设备 ID；
2. 再从 `devices` 表中筛出这些 ID 对应的设备。

这类子查询特别适合表达“是否属于某个结果集合”。

### 深入理解：为什么有时用子查询比 JOIN 更直观

很多需求既可以用 JOIN，也可以用子查询实现。什么时候更适合子查询？

当你的思路是：

1. 先求出一个统计值；
2. 或先求出一个 ID 集合；
3. 再拿这个结果去做外层筛选；

那么子查询往往更符合人的思维顺序，也更接近自然语言描述。

例如“查出温度高于平均值的记录”，如果用子查询，SQL 就和这句话几乎一一对应。

### `IN` 和 `EXISTS` 的思路区别

虽然本章示例使用的是 `IN`，但值得顺手建立一个概念：

1. `IN (子查询)` 更像“某个值是否属于这个结果集合”；
2. `EXISTS (子查询)` 更像“是否存在满足条件的记录”。

对初学阶段来说，先掌握 `IN` 已经足够；等写更复杂的关联筛选时，再考虑 `EXISTS`。

## 4.3 索引（INDEX）

### 什么是索引？

**索引**类似于一本书的目录。没有索引时，查询数据需要扫描整个表（全表扫描）；有了索引，数据库可以快速定位到目标数据。

```
无索引（全表扫描）：              有索引（B-Tree 查找）：
┌──┬──┬──┬──┬──┬──┬──┬──┐     ┌──────────┐
│  │  │  │  │  │  │  │✓ │     │   根节点   │
└──┴──┴──┴──┴──┴──┴──┴──┘     └────┬─────┘
扫描8次才找到目标                    │ 比较
                              ┌────┴─────┐
                              │  中间节点  │
                              └────┬─────┘
                                   │ 比较
                              ┌────┴─────┐
                              │  ✓ 找到！ │
                              └──────────┘
                              仅需3次比较
```

### 嵌入式类比

索引就像 Flash 中的**查找表（LUT）**——用额外的存储空间换取查找速度。

更准确地说，数据库索引通常不是简单数组，而是某种适合范围查找和排序的树结构，SQLite 默认使用 B-Tree 索引。

对使用者来说，不必先深入底层实现，也可以先掌握一个实用心智模型：

> 索引的本质，是为数据库提前准备一份“按某个键排好序的快速查找结构”。

### 何时创建索引？

| 场景 | 是否需要索引 |
|------|-------------|
| 频繁出现在 WHERE 子句中的列 | ✅ 是 |
| 经常用于 JOIN 关联的列 | ✅ 是 |
| 经常用于 ORDER BY 排序的列 | ✅ 是 |
| 很少查询、经常更新的列 | ❌ 否（索引会降低写入速度） |
| 值重复率很高的列（如性别） | ❌ 否（索引效果差） |

```sql
-- 创建索引
CREATE INDEX idx_sensor_device_id ON sensor_data(device_id);
CREATE INDEX idx_sensor_timestamp ON sensor_data(timestamp);

-- 复合索引（多列）
CREATE INDEX idx_sensor_device_time ON sensor_data(device_id, timestamp);

-- 查看执行计划（验证索引是否被使用）
EXPLAIN QUERY PLAN SELECT * FROM sensor_data WHERE device_id = 1;
```

### 深入理解：为什么索引会提升查询速度

假设你要查：

```sql
SELECT * FROM sensor_data WHERE device_id = 1;
```

如果没有索引，数据库通常只能：

1. 从第一行开始看；
2. 判断 `device_id` 是否等于 1；
3. 不等就继续看下一行；
4. 一直扫完整张表。

这就是全表扫描。

而当 `device_id` 上有索引时，数据库就不必一行一行地盲找，而是可以借助索引快速定位到目标记录所在区域，再读取对应行。

### 为什么索引不是越多越好

索引会提升查询性能，但它并不是“只赚不亏”。

每增加一个索引，都意味着：

1. 占用更多存储空间；
2. 插入、更新、删除数据时，索引也要同步维护；
3. 写入成本会增加。

所以工程上真正重要的不是“有没有索引”，而是“是否把索引建在最有价值的列上”。

### `EXPLAIN QUERY PLAN` 是怎么帮你验证索引的

本章示例代码做了一件非常重要的事情：不是停留在“创建索引”这一步，而是继续查看执行计划。

示例代码会先执行：

```sql
EXPLAIN QUERY PLAN SELECT * FROM sensor_data WHERE device_id = 1;
```

然后再创建索引：

```sql
CREATE INDEX idx_sensor_device_id ON sensor_data(device_id);
```

再执行同样的 `EXPLAIN QUERY PLAN`。

这样你就能对比：

1. 没有索引时，查询计划是否显示为全表扫描；
2. 有索引后，查询计划是否显示为使用索引搜索。

### 如何读懂 `EXPLAIN QUERY PLAN` 的结果

SQLite 的执行计划结果通常会包含一列 `detail`，里面会出现类似这样的描述：

1. `SCAN sensor_data`
2. `SEARCH sensor_data USING INDEX idx_sensor_device_id (...)`

你可以先按非常实用的方式理解这两个关键词：

1. **`SCAN`**：扫表，通常意味着没有高效利用索引。
2. **`SEARCH`**：按条件查找，通常说明索引起作用了。

对初学者来说，先能看懂 `SCAN` 和 `SEARCH` 的区别，已经足够完成大部分索引验证工作。

### 复合索引应该怎么理解

文档里提到了：

```sql
CREATE INDEX idx_sensor_device_time ON sensor_data(device_id, timestamp);
```

这叫复合索引，也就是一个索引同时按多列组织。

它适合这种查询：

```sql
WHERE device_id = ? AND timestamp > ?
ORDER BY timestamp
```

对初学者最值得记住的一点是：

> 复合索引的使用效果通常和列顺序有关。

也就是说，`(device_id, timestamp)` 和 `(timestamp, device_id)` 不是完全等价的设计。

## 4.4 视图（VIEW）

视图是一个**虚拟表**，它是一个预定义的 SELECT 查询。视图不存储数据，每次查询时实时计算。

```sql
-- 创建视图：设备数据统计概览
CREATE VIEW device_summary AS
SELECT d.device_id, d.name, d.type,
       COUNT(s.id) as total_readings,
       AVG(s.temperature) as avg_temp,
       MAX(s.temperature) as max_temp,
       MIN(s.temperature) as min_temp
FROM devices d
LEFT JOIN sensor_data s ON d.device_id = s.device_id
GROUP BY d.device_id;

-- 使用视图就像使用普通表一样
SELECT * FROM device_summary WHERE avg_temp > 25.0;
```

### 深入理解：为什么视图适合封装复杂查询

假设你经常需要查看“每台设备的读数条数、平均温度、最高温度、最低温度”。如果每次都手写一长串 `LEFT JOIN + GROUP BY + COUNT + AVG + MAX + MIN`，就会有两个问题：

1. SQL 很长，阅读成本高；
2. 同样的逻辑会在多个地方重复。

这时就可以把这段查询定义成视图：

```sql
CREATE VIEW device_summary AS ...
```

之后再查询时，就可以像查询普通表一样写：

```sql
SELECT * FROM device_summary;
```

因此，视图最重要的价值不是“性能”，而是：

> 封装复杂查询逻辑，降低重复书写和理解成本。

### 视图和普通表的区别

虽然视图看起来像表，但它和真实表并不一样：

1. 普通表真正存储数据；
2. 视图只存储查询定义；
3. 访问视图时，数据库会按视图定义去计算结果。

所以可以把视图理解成：

> 一个有名字的、可重复使用的查询结果模板。

### 示例中的 `device_summary` 视图在做什么

示例代码中创建的视图：

```sql
CREATE VIEW device_summary AS
SELECT d.device_id, d.name, d.type,
                COUNT(s.id) as total_readings,
                ROUND(AVG(s.temperature), 1) as avg_temp,
                ROUND(MAX(s.temperature), 1) as max_temp,
                ROUND(MIN(s.temperature), 1) as min_temp
FROM devices d
LEFT JOIN sensor_data s ON d.device_id = s.device_id
GROUP BY d.device_id;
```

它本质上是在做“设备统计概览表”：

1. 每台设备只输出一行；
2. 展示设备基础信息；
3. 同时附带这台设备的统计指标。

这种视图在真实项目里非常常见，因为它很适合作为报表页、监控页、后台管理页的数据来源。

### 视图的一个重要使用习惯

视图适合封装“稳定、通用、会被重复使用”的查询逻辑。

如果某段 SQL：

1. 已经很长；
2. 会在多个地方用到；
3. 语义比较稳定；

那么很值得考虑把它抽成视图。

如果只是一次性临时查询，就没必要强行创建视图。

## 4.5 HAVING 与 CASE 表达式

前面几节介绍了 JOIN、子查询、索引和视图，但本章示例代码里其实还演示了两个非常常用的高级查询技巧：`HAVING` 和 `CASE`。

它们都不是单独的“查询类型”，而是让 SQL 表达能力更强的关键语法。

### `HAVING`：筛选分组后的结果

示例中的查询：

```sql
SELECT d.name, ROUND(AVG(s.temperature), 1) as avg_temp, COUNT(*) as cnt
FROM sensor_data s
JOIN devices d ON s.device_id = d.device_id
GROUP BY s.device_id
HAVING avg_temp > 25.0;
```

它表达的是：

1. 先按设备分组；
2. 对每组计算平均温度和条数；
3. 再只保留平均温度大于 25 的设备。

这里最重要的是区分 `WHERE` 和 `HAVING`：

1. `WHERE` 是在分组前筛选原始记录；
2. `HAVING` 是在分组后筛选聚合结果。

因此：

```sql
WHERE temperature > 25.0
```

和：

```sql
HAVING AVG(temperature) > 25.0
```

不是一回事。前者是“只拿高于 25 的明细行”，后者是“只保留平均值高于 25 的设备分组”。

### `CASE`：在查询里做条件判断

示例中的查询：

```sql
SELECT device_id, temperature,
CASE
     WHEN temperature >= 27.0 THEN '高温'
     WHEN temperature >= 25.0 THEN '正常'
     ELSE '低温'
END as level
FROM sensor_data
ORDER BY temperature DESC;
```

这里的 `CASE` 可以理解成 SQL 里的 `if / else if / else`。

它的执行顺序是：

1. 先判断是否 `temperature >= 27.0`；
2. 如果是，返回 `'高温'`；
3. 否则继续判断是否 `temperature >= 25.0`；
4. 如果是，返回 `'正常'`；
5. 都不满足时返回 `'低温'`。

于是查询结果中就多出了一列 `level`，把原始温度值转换成更适合展示和业务判断的等级标签。

### `CASE` 常见用途

在实际项目中，`CASE` 很适合做：

1. 状态码转文字说明；
2. 分数分档；
3. 温度、电压、告警级别分类；
4. 报表中按条件生成标签列。

因此它是非常实用的“数据展示层转换工具”。

## 4.6 示例代码讲解

完整示例：[examples/ch04_advanced_query.cpp](../examples/ch04_advanced_query.cpp)

运行方式：

```bash
cd build && ./ch04_advanced_query
```

### 代码结构总览

这个示例程序整体分成三部分：

1. `prepare_data()`：创建测试表并插入示例数据；
2. `run_query()`：统一执行查询并把结果按表格形式打印；
3. `main()`：按专题依次演示 JOIN、子查询、索引、视图、HAVING、CASE。

### `run_query()` 为什么很值得学习

这个函数虽然不长，但它把“通用查询打印器”的基本思路都展示出来了。

#### 1. `sqlite3_prepare_v2()` 预编译查询

```cpp
int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
```

这一步和第三章一致，是把 SQL 文本编译成语句对象。

#### 2. `sqlite3_column_count()` 获取结果列数

```cpp
int col_count = sqlite3_column_count(stmt);
```

它返回当前查询结果一共有多少列。这样后面的打印逻辑就不需要预先写死列数，而是可以适配不同 SQL。

#### 3. `sqlite3_column_name()` 读取列名

```cpp
sqlite3_column_name(stmt, i)
```

它读取的是结果集第 `i` 列的列名。注意这里读到的也是“查询结果列名”，所以如果 SQL 使用了别名，例如：

```sql
COUNT(s.id) as data_count
```

那么打印出来的列名就是 `data_count`。

#### 4. `sqlite3_step()` + `sqlite3_column_text()` 逐行打印

```cpp
while (sqlite3_step(stmt) == SQLITE_ROW) {
          for (int i = 0; i < col_count; i++) {
                    const char *val = (const char*)sqlite3_column_text(stmt, i);
                    printf("%-16s", val ? val : "NULL");
          }
}
```

这里的写法有一个很实用的教学价值：它把所有结果列都统一按文本打印出来，因此不必为每个查询单独写 `sqlite3_column_int()`、`sqlite3_column_double()` 的解析逻辑。

这适合用来做：

1. 教学演示；
2. 调试输出；
3. 通用 SQL 查询结果查看工具。

当然，正式业务代码里，如果你明确知道列类型，通常还是更推荐用 `sqlite3_column_int()`、`sqlite3_column_double()` 等更精确的读取方式。

### 为什么这个示例比第三章更像“分析型查询”

第三章的重点是 CRUD 基础操作，而本章示例开始明显转向“分析与汇总”：

1. `JOIN` 把分散数据拼起来；
2. `GROUP BY + AVG/COUNT` 做统计；
3. 子查询表达“依赖其他查询结果的条件”；
4. 视图把统计逻辑封装起来；
5. `HAVING` 和 `CASE` 进一步增强表达能力。

这也是为什么第四章在 SQL 学习路径里很关键：它标志着你开始从“会增删改查”走向“会写有业务含义的数据分析查询”。

## 4.7 本章小结

- **JOIN** 将多个表按关联条件合并查询
- **子查询** 可以嵌套在 WHERE、FROM、SELECT 子句中
- **索引** 是用空间换时间的优化手段，对 WHERE/JOIN/ORDER BY 列创建索引
- **视图** 是预定义的查询，简化复杂 SQL 的使用
- **EXPLAIN QUERY PLAN** 可以查看 SQL 的执行计划，验证索引效果

### 把本章浓缩成一套心智模型

如果要把第四章压缩成最核心的几句话，可以记成：

1. `JOIN` 解决“多张表的信息怎么拼起来”。
2. 子查询解决“一个查询条件依赖另一个查询结果”。
3. 索引解决“同样的查询怎么更快”。
4. 视图解决“复杂查询怎么复用”。
5. `HAVING` 和 `CASE` 解决“统计结果怎么筛选、原始值怎么分类展示”。

### 从本章进入下一章前，你应该已经掌握什么

如果本章内容已经掌握，你应该能够：

1. 看懂并编写基本的 `INNER JOIN` 和 `LEFT JOIN`。
2. 用子查询表达“高于平均值”“属于某个集合”这类条件。
3. 知道如何用 `EXPLAIN QUERY PLAN` 验证索引是否被使用。
4. 明白视图适合封装稳定、重复使用的复杂查询。
5. 区分 `WHERE` 与 `HAVING`，并能用 `CASE` 给结果增加分类列。

这意味着你已经具备继续学习事务、并发和更深层数据库行为的基础。

---

[⬅ 第三章：SQL 基本操作](ch03_basic_sql.md) | [目录](../README.md) | [第五章：事务与并发控制 ➡](ch05_transaction.md)
