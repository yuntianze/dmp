# DMP 风控系统 Cursor AI + Claude API 协同工作流

## 核心策略 (基于当前项目状态)

- **Cursor AI (80%)**: 日常开发、代码完善、调试、重构
- **Claude API (20%)**: 复杂架构、核心算法、性能关键代码

## 项目当前状态分析

基于 `@dmp-system-design.md` 和现有代码，项目已完成：

✅ **已完成基础设施**:
- 项目结构完整 (include/src/config/models/data 等)
- CMake 构建系统配置 (支持 C++20, 高性能编译选项)
- 核心类型定义 (`types.hpp`, `transaction.hpp`)
- HTTP 服务器基础架构 (Drogon + simdjson)
- 配置文件模板 (TOML/JSON/YAML 格式)
- 性能测试脚本和部署脚本

⚠️  **需要实现的核心组件**:
- 规则引擎 (ExprTk 基础)
- 模式匹配器 (Hyperscan 集成)
- 特征提取和缓存系统
- ML 推理引擎 (ONNX Runtime)
- 决策融合逻辑
- 监控和指标收集

## 阶段化实施策略

### 🚀 第一阶段：完善 HTTP 服务器 (主要用 Cursor AI)

**当前状态**: HTTP 服务器基础框架已搭建，需要完善核心处理逻辑

#### Task 1-1: 完善 TransactionRequest 解析
```
@transaction.hpp @handlers.cpp 
完善 TransactionRequest::from_json() 方法实现：

1. 使用 simdjson 高性能解析
2. 完整的字段验证和错误处理
3. 支持嵌套对象解析 (transaction/card/device/customer)
4. 添加性能指标统计 (解析耗时)
5. 符合 Google C++ 代码注释规范 (英文注释)

参考设计文档中的数据契约格式，确保所有字段正确映射
```

#### Task 1-2: 增强错误处理和响应
```
@handlers.cpp @health.cpp
完善 HTTP 处理器：

1. 标准化错误码和错误消息
2. 添加请求限流和超时控制
3. 完善健康检查逻辑 (检查依赖组件状态)
4. 添加请求 ID 追踪和日志关联
5. 实现优雅的异常捕获和恢复

目标：P99 延迟 < 5ms (仅 HTTP 层)
```

#### Task 1-3: 集成 Prometheus 监控
```
创建 src/monitor/http_metrics.cpp：

使用现有的 prometheus-cpp 依赖：
1. 请求延迟直方图 (histogram)
2. 请求总数计数器 (counter)
3. 错误率统计
4. 并发连接数 (gauge)

在每个 HTTP 处理器中埋点，确保监控数据准确
```

### ⚡ 第二阶段：规则引擎核心 (混合策略)

**策略**: 接口设计用 Cursor AI，核心算法用 Claude API

#### Task 2-1: 规则引擎接口设计 (Cursor AI)
```
@rule_engine.hpp 设计规则引擎接口：

基于 config/rules.json 现有配置格式：
1. Rule 结构体定义 (id, expression, weight, enabled)
2. RuleEngine 类接口声明
3. RuleResult 和 RuleMetrics 定义
4. 线程安全的规则加载和热重载接口
5. 性能监控接口

重点关注：类型安全、异常处理、扩展性
```

#### Task 2-2: ExprTk 规则引擎实现 (Claude API) 🔥
```bash
# 关键性能组件，使用 Claude API
claude create src/engine/rule_engine.cpp --prompt "
基于现有接口和 ExprTk 库实现高性能规则引擎：

性能要求：
- 规则编译后缓存，避免重复解析
- 支持 thread_local 存储，减少锁竞争
- 目标：1000 条规则评估 < 5ms
- 支持规则优先级排序，热规则优先

功能要求：
1. 从 config/rules.json 加载规则
2. ExprTk 表达式编译和缓存
3. 批量规则评估 (针对单个交易)
4. 规则命中统计和性能监控
5. 热重载机制 (文件变化监听)

技术要求：
- 异常安全的 RAII 设计
- 内存池优化
- 完整的错误处理和降级策略
- 英文注释，遵循 Google C++ 风格

生成完整的生产级实现代码
"
```

#### Task 2-3: 模式匹配器实现 (Cursor AI)
```
@pattern_matcher.cpp 创建基于 Hyperscan 的模式匹配器：

基于 data/blocklist.txt 和 data/whitelist.txt：
1. PatternMatcher 类实现
2. 正则表达式数据库预编译
3. IP 地址、商户 ID、设备指纹匹配
4. 批量匹配优化 (减少系统调用)
5. 缓存匹配结果

重点：利用 Hyperscan 的 SIMD 优化特性
```

### 🧠 第三阶段：特征工程和缓存 (混合策略)

#### Task 3-1: 特征提取器设计 (Cursor AI)
```
基于 config/features.yaml 实现特征提取：

@feature/extractor.cpp 创建 FeatureExtractor 类：
1. 从 TransactionRequest 提取基础特征
2. 支持数值标准化 (log_scale, min-max)
3. 分类特征编码 (one-hot, label encoding)
4. 时序特征计算 (时间周期、间隔)
5. 输出固定长度特征向量 (64 维)

确保与 ML 模型输入格式兼容
```

#### Task 3-2: 多级缓存系统 (Claude API) 🔥
```bash
# 性能关键组件，使用 Claude API
claude create src/feature/cache.cpp include/feature/cache.hpp --prompt "
使用 parallel-hashmap 实现高性能多级缓存系统：

基于 config/features.yaml 的缓存配置：
- L1: thread_local 缓存 (16MB, 60s TTL)
- L2: 进程级 LRU 缓存 (256MB, 300s TTL) 
- L3: Redis 缓存接口 (1GB, 3600s TTL)

性能目标：
- L1 命中延迟 < 0.1ms
- L2 命中延迟 < 1ms
- 支持 10,000+ TPS 并发访问
- 内存使用可控，支持优雅降级

技术要求：
1. FeatureCache 类设计
2. 使用 phmap::parallel_flat_hash_map
3. 线程安全的 LRU 实现
4. Redis 异步接口 (可选)
5. 缓存预热和失效机制
6. 完整的性能指标收集

生成企业级缓存系统实现
"
```

#### Task 3-3: 聚合特征计算 (Cursor AI)
```
@feature/aggregator.cpp 实现时序聚合特征：

基于设计文档的特征类型：
1. 滑动窗口聚合 (1h, 24h, 7d)
2. 计数、求和、平均值、分位数
3. 用户/商户维度的历史统计
4. 异步计算和结果缓存
5. 降级策略 (缓存未命中时的默认值)

集成到特征提取管线中
```

### 🤖 第四阶段：ML 推理引擎 (混合策略)

#### Task 4-1: 模型管理器接口 (Cursor AI)
```
@inference/model_manager.hpp 设计模型管理接口：

基于 config/models.toml：
1. ModelManager 类接口
2. 多模型支持 (primary/secondary)
3. 模型版本管理和 A/B 测试
4. 推理会话池管理
5. 性能监控接口

支持模型热加载和回滚机制
```

#### Task 4-2: ONNX Runtime 推理引擎 (Claude API) 🔥
```bash
# ML 推理性能关键，使用 Claude API
claude create src/inference/predictor.cpp --prompt "
使用 ONNX Runtime 实现高性能 ML 推理引擎：

基于现有 CMake 配置 (ONNX Runtime 可选依赖)：

性能要求：
- 单次推理延迟 < 10ms
- 支持批量推理 (batch_size=32)
- 内存使用优化 (arena allocator)
- 支持多线程推理

功能要求：
1. Predictor 类实现
2. 特征向量预处理
3. ONNX 模型推理
4. 结果后处理和概率校准
5. 推理失败降级 (使用规则引擎结果)
6. 详细的性能监控

技术要求：
- 异常安全设计
- 资源 RAII 管理
- 支持 CPU 和 GPU 提供商
- 完整的错误处理
- 英文注释，符合 Google 风格

生成生产级推理引擎实现
"
```

#### Task 4-3: 模型热更新机制 (Cursor AI)
```
@inference/model_updater.cpp 实现模型热更新：

1. 文件系统监听 (models/ 目录)
2. 模型文件验证和加载
3. 原子性模型切换
4. 版本回滚机制
5. 更新状态通知

确保更新过程中服务不中断
```

### 🔀 第五阶段：决策融合和集成 (Claude API) 🔥

#### Task 5-1: 决策融合引擎 (Claude API)
```bash
# 系统核心逻辑，使用 Claude API
claude create src/engine/decision_fusion.cpp --prompt "
实现智能决策融合引擎：

输入源：
1. 规则引擎评分和触发规则
2. 模式匹配结果 (黑白名单)
3. ML 模型预测概率
4. 历史决策上下文

融合策略：
1. 加权投票算法
2. 规则优先级处理
3. 置信度阈值判断
4. 决策原因生成和追踪

基于 config/rules.json 的阈值配置：
- approve_threshold: 30.0
- review_threshold: 70.0

输出：Decision (APPROVE/DECLINE/REVIEW) + 详细原因

性能要求：
- 融合计算 < 1ms
- 支持决策解释性
- 异常情况降级策略

生成完整的决策融合实现
"
```

#### Task 5-2: 主服务集成 (Claude API)
```bash
# 系统集成关键代码，使用 Claude API
claude create src/main.cpp --prompt "
基于现有代码框架，完善主服务集成：

组件初始化顺序：
1. 配置加载 (server.toml, rules.json, features.yaml, models.toml)
2. 日志系统初始化 (spdlog)
3. 监控系统启动 (prometheus-cpp)
4. 缓存系统初始化
5. 规则引擎加载
6. ML 模型加载
7. HTTP 服务器启动

性能优化：
- 组件预热 (缓存预加载，模型预推理)
- 内存预分配
- 线程池优化配置
- 系统资源监控

错误处理：
- 启动失败时的组件回滚
- 运行时异常恢复
- 优雅关闭处理
- 资源清理保证

目标：启动时间 < 10s，内存使用 < 2GB

生成生产级主服务实现
"
```

## Cursor AI 最佳实践 (基于现有项目)

### 1. 利用现有上下文
```
@dmp-system-design.md @transaction.hpp 
基于设计文档和现有类型定义，帮我实现...

@workspace 
参考整个项目的配置文件和头文件结构，生成...

@config/rules.json @include/common/types.hpp 
基于现有配置格式，实现规则解析逻辑...
```

### 2. 渐进式开发
```
// 第一步：基于现有接口扩展
"@handlers.cpp 添加决策逻辑调用接口，先创建空实现"

// 第二步：实现核心逻辑
"实现特征提取的基础数值处理部分"

// 第三步：性能优化
"优化缓存命中率，添加预加载机制"
```

### 3. 代码质量保障
```
"检查这段代码的线程安全性"
"优化这个函数的内存分配"
"添加 Google C++ 风格的英文注释"
"生成对应的单元测试，使用 GoogleTest"
"确保符合 dmp-rule.mdc 编码规范"
```

### 4. 性能导向开发
```
"这段代码如何优化到 P99 < 50ms？"
"分析内存热点，建议优化方案"
"检查是否充分利用了 SIMD 指令"
"评估缓存友好性，建议数据结构改进"
```

## Claude API 使用策略 (精准投入)

### 何时使用 Claude API (20% 关键场景)
1. **核心算法实现**: 规则引擎、缓存系统、决策融合
2. **性能关键路径**: ML 推理、特征计算、热点优化
3. **复杂系统集成**: 主函数、组件编排、错误处理
4. **架构设计决策**: 线程模型、内存管理、异步处理

### 成本控制策略
1. **需求明确化**: 基于现有代码和配置，提供详细上下文
2. **一次到位**: 生成完整可用的实现，减少迭代成本
3. **重点投入**: 仅在影响系统性能的关键组件使用

## 开发里程碑 (基于当前进度)

### 第1-2天：HTTP 服务完善
- [x] 基础框架搭建 ✅
- [ ] JSON 解析优化
- [ ] 错误处理增强
- [ ] 监控集成

### 第3-4天：规则引擎
- [ ] ExprTk 规则引擎 (Claude API)
- [ ] Hyperscan 模式匹配
- [ ] 规则热重载机制

### 第5-6天：特征和缓存
- [ ] 特征提取实现
- [ ] 多级缓存系统 (Claude API)
- [ ] 聚合特征计算

### 第7-8天：ML 推理
- [ ] ONNX Runtime 集成 (Claude API)
- [ ] 模型管理和热更新
- [ ] 推理性能优化

### 第9-10天：集成和优化
- [ ] 决策融合引擎 (Claude API)
- [ ] 主服务集成 (Claude API)
- [ ] 端到端性能调优

## 质量控制检查点

基于现有脚本和工具：

### 编译测试
```bash
# 使用现有构建脚本
./scripts/build.sh Debug    # 开发调试
./scripts/build.sh Release  # 生产优化
```

### 性能验证
```bash
# 使用现有性能测试脚本
python3 scripts/benchmark.py --requests 10000 --concurrency 100

# SLO 目标验证
# P99 ≤ 50ms, QPS ≥ 10,000
```

### 代码质量检查
```bash
# 内存安全检查 (已配置 sanitizer)
./build_debug/dmp_server  # AddressSanitizer + UBSan

# 性能分析
perf record ./build_release/dmp_server
perf report
```

### 功能测试
```bash
# 健康检查
curl http://localhost:8080/health

# 决策接口测试
curl -X POST http://localhost:8080/api/v1/decision \
  -H "Content-Type: application/json" \
  -d @test_request.json
```

## 项目特色优势

基于现有架构，此工作流充分利用了：

1. **现代 C++ 特性**: C++20 协程、SIMD 优化、模板元编程
2. **高性能库栈**: Drogon + simdjson + ExprTk + Hyperscan
3. **完整工程化**: CMake + 脚本 + 配置 + 监控
4. **性能导向**: 编译优化 + 缓存设计 + 异步处理

这个工作流确保在控制 Claude API 成本的同时，通过 Cursor AI 处理日常开发，在关键性能组件上精准投入 Claude API，实现最佳的开发效率和代码质量。
