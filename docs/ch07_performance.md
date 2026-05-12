# 第七章：性能优化与最佳实践

## 7.1 SQLite 性能优化核心原则

对于嵌入式系统，性能优化通常聚焦于两个方面：**减少磁盘 I/O** 和 **减少 CPU 计算**。

```
性能瓶颈排名：

  磁盘同步 (fsync)  >>>>  磁盘读写  >>  CPU 计算  >  内存分配
  (最慢，差距巨大)        (较慢)        (较快)       (最快)
```

## 7.2 关键优化手段

### 1. 使用事务包裹批量操作

这是**最重要的优化**，可以提升 10-100 倍性能（第五章已详细介绍）。

### 2. 预编译语句复用

```cpp
// ❌ 每次都编译 SQL（慢）
for (int i = 0; i < 1000; i++) {
    sqlite3_exec(db, "INSERT ...", ...);  // 每次都编译+执行
}

// ✅ 编译一次，执行多次（快）
sqlite3_prepare_v2(db, "INSERT ... VALUES (?, ?)", ...);
for (int i = 0; i < 1000; i++) {
    sqlite3_bind_*(...);
    sqlite3_step(stmt);
    sqlite3_reset(stmt);  // 重置后复用
}
sqlite3_finalize(stmt);
```

### 3. 合理使用索引

- 对 WHERE、JOIN、ORDER BY 中频繁出现的列创建索引
- 避免过多索引（每个索引都会降低写入速度）

### 4. PRAGMA 调优

```sql
-- 设置缓存大小（页面数，默认 -2000 即约 2MB）
PRAGMA cache_size = -8000;  -- 约 8MB 缓存

-- 同步模式（影响数据安全性 vs 性能）
PRAGMA synchronous = NORMAL;  -- 性能与安全的平衡
-- FULL: 最安全但最慢（默认）
-- NORMAL: 推荐，极小概率丢失数据
-- OFF: 最快但断电可能损坏数据库

-- 临时存储位置
PRAGMA temp_store = MEMORY;  -- 临时表和索引放在内存中

-- 内存映射 I/O
PRAGMA mmap_size = 268435456;  -- 256MB 内存映射
```

### 5. 数据库维护

```sql
-- 回收已删除数据的空间
VACUUM;

-- 分析表统计信息（帮助查询优化器选择最佳执行计划）
ANALYZE;

-- 完整性检查
PRAGMA integrity_check;
```

## 7.3 嵌入式场景的特殊考量

| 场景 | 建议 |
|------|------|
| Flash 存储（SD卡、eMMC） | 使用 WAL 模式减少写放大 |
| 内存受限 | 减小 `cache_size`，使用 `temp_store = FILE` |
| 频繁写入 | 合并写操作到事务中，使用 `synchronous = NORMAL` |
| 数据量大 | 创建适当索引，定期 VACUUM |
| 需要高可靠性 | 保持 `synchronous = FULL`，定期备份 |

## 7.4 示例代码

完整示例：[examples/ch07_performance.cpp](../examples/ch07_performance.cpp)

运行方式：

```bash
cd build && ./ch07_performance
```

## 7.5 本章小结

性能优化优先级（从高到低）：

1. **使用事务** —— 最大的性能提升
2. **预编译语句复用** —— 避免重复编译 SQL
3. **创建索引** —— 加速查询
4. **PRAGMA 调优** —— synchronous、cache_size、WAL
5. **定期维护** —— VACUUM、ANALYZE

---

[⬅ 第六章：C/C++ API 详解](ch06_api_detail.md) | [目录](../README.md) | [第八章：嵌入式实战 ➡](ch08_embedded.md)
