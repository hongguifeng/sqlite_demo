# 第四章：高级查询

本章介绍更强大的 SQL 查询技巧，包括多表关联（JOIN）、子查询、索引和视图。

## 4.1 多表关联（JOIN）

在实际项目中，数据通常分散在多个表中。**JOIN** 操作可以将多个表的数据按照关联条件合并在一起。

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

## 4.5 示例代码

完整示例：[examples/ch04_advanced_query.cpp](../examples/ch04_advanced_query.cpp)

运行方式：

```bash
cd build && ./ch04_advanced_query
```

## 4.6 本章小结

- **JOIN** 将多个表按关联条件合并查询
- **子查询** 可以嵌套在 WHERE、FROM、SELECT 子句中
- **索引** 是用空间换时间的优化手段，对 WHERE/JOIN/ORDER BY 列创建索引
- **视图** 是预定义的查询，简化复杂 SQL 的使用
- **EXPLAIN QUERY PLAN** 可以查看 SQL 的执行计划，验证索引效果

---

[⬅ 第三章：SQL 基本操作](ch03_basic_sql.md) | [目录](../README.md) | [第五章：事务与并发控制 ➡](ch05_transaction.md)
