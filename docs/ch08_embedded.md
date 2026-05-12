# 第八章：嵌入式场景实战

本章将综合前面所学，实现一个面向嵌入式场景的完整数据采集管理系统。

## 8.1 场景描述

假设你正在开发一个 IoT 网关设备，需要：

1. 管理多个传感器设备的注册信息
2. 采集并存储传感器数据（温度、湿度）
3. 设置告警规则并记录告警事件
4. 定期清理过期数据
5. 支持数据查询和统计

## 8.2 数据库设计

```
┌──────────────┐       ┌──────────────────┐
│   devices    │       │   sensor_data     │
│──────────────│       │──────────────────│
│ device_id PK │──┐    │ id PK            │
│ name         │  │    │ device_id FK ─────┤
│ type         │  └───→│ temperature      │
│ location     │       │ humidity          │
│ status       │       │ timestamp        │
│ created_at   │       └──────────────────┘
└──────────────┘
        │              ┌──────────────────┐
        │              │   alert_rules     │
        └─────────────→│ id PK            │
                       │ device_id FK     │
                       │ metric           │
                       │ threshold        │
                       │ condition        │
                       └──────────────────┘
                               │
                       ┌───────┴──────────┐
                       │   alert_events    │
                       │ id PK            │
                       │ rule_id FK       │
                       │ device_id FK     │
                       │ value            │
                       │ timestamp        │
                       └──────────────────┘
```

## 8.3 实现要点

### 数据库初始化封装

在嵌入式项目中，建议将数据库操作封装为模块化的函数或类：

```cpp
class SensorDatabase {
public:
    bool open(const char *path);
    void close();
    
    // 设备管理
    int addDevice(const char *name, const char *type, const char *location);
    bool removeDevice(int device_id);
    
    // 数据采集
    bool insertReading(int device_id, double temp, double humidity);
    
    // 告警
    bool checkAlerts(int device_id, double temp, double humidity);
    
    // 维护
    int cleanOldData(int days);
};
```

### 自动清理策略

嵌入式设备存储有限，需要定期清理旧数据：

```sql
-- 删除 30 天前的数据
DELETE FROM sensor_data 
WHERE timestamp < datetime('now', '-30 days');

-- 限制总记录数（保留最新的 N 条）
DELETE FROM sensor_data 
WHERE id NOT IN (
    SELECT id FROM sensor_data 
    ORDER BY timestamp DESC 
    LIMIT 100000
);
```

## 8.4 示例代码

完整示例：[examples/ch08_embedded.cpp](../examples/ch08_embedded.cpp)（约 400 行，包含完整的 `SensorDatabase` 类实现）

运行方式：

```bash
cd build && ./ch08_embedded
```

## 8.5 生产环境建议

### 文件系统注意事项

| 文件系统 | SQLite 兼容性 | 注意事项 |
|----------|--------------|---------|
| ext4     | ✅ 完全兼容   | 推荐 |
| FAT32    | △ 基本兼容    | 不支持文件锁，单进程使用 |
| tmpfs    | ✅ 兼容       | 掉电数据丢失 |
| NFS      | ❌ 不推荐     | 锁机制不可靠 |

### 嵌入式部署清单

1. ✅ 使用 WAL 模式
2. ✅ 设置 `synchronous = NORMAL`
3. ✅ 设置合理的 `cache_size`
4. ✅ 批量操作使用事务
5. ✅ 定期清理旧数据
6. ✅ 定期运行 `PRAGMA integrity_check`
7. ✅ 实现数据库备份机制
8. ✅ 使用参数绑定防止 SQL 注入

## 8.6 本章小结

- 嵌入式场景下 SQLite 应封装为模块化接口
- 数据清理策略是长期运行设备的必要功能
- WAL 模式 + `synchronous = NORMAL` 是嵌入式推荐配置
- 告警系统可以通过触发器或应用层逻辑实现
- 统计查询可用视图简化

## 教程总结

恭喜完成本教程！你已经学会了：

1. **数据库基础**：表、行、列、SQL 语言
2. **SQLite 特性**：零配置、单文件、进程内
3. **CRUD 操作**：CREATE TABLE, INSERT, SELECT, UPDATE, DELETE
4. **高级查询**：JOIN, 子查询, 索引, 视图
5. **事务**：ACID 特性, BEGIN/COMMIT/ROLLBACK, SAVEPOINT
6. **C API 详解**：预编译语句, BLOB, 自定义函数
7. **性能优化**：PRAGMA 调优, 批量事务, 索引策略
8. **嵌入式实战**：数据采集系统的完整实现

---

[⬅ 第七章：性能优化](ch07_performance.md) | [目录](../README.md)
