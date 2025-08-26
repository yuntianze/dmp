# DMP 项目分阶段实施计划

## 阶段 1：基础框架 (1-2 天)

### 目标
- 建立可编译的基础框架
- 实现简单的 HTTP API
- 验证基础架构

### Claude Code 任务序列

1. **初始化项目结构**
   ```bash
   # 运行前面提供的初始化脚本
   ./init_project.sh
   ```

2. **生成核心类型定义**
   ```bash
   claude-code create include/common/types.hpp --prompt "
   创建 C++20 核心数据结构，包含：
   - Transaction 完整结构体（嵌套的 transaction、card、device、customer）
   - Decision 枚举（APPROVE, DECLINE, REVIEW）
   - Features 特征向量结构
   - 所有结构都支持 JSON 序列化
   使用 std::string、std::vector、std::array
   "
   ```

3. **生成基础配置管理**
   ```bash
   claude-code create include/common/config.hpp src/common/config.cpp --prompt "
   使用 toml++ 创建配置管理类，支持：
   - 服务器配置（端口、线程数）
   - 数据库配置  
   - 缓存配置
   - 性能参数配置
   包含配置热重载功能
   "
   ```

4. **生成简单 HTTP 服务器**
   ```bash
   claude-code create src/server/server.cpp src/server/handlers.cpp --prompt "
   使用 Drogon 框架创建 HTTP 服务器：
   - POST /api/v1/decision 端点
   - 支持 JSON 请求解析和响应
   - 基础的错误处理和日志
   - 简单返回固定决策结果用于测试
   "
   ```

5. **生成主函数**
   ```bash
   claude-code create src/main.cpp --prompt "
   创建主函数：
   - 加载配置
   - 初始化日志
   - 启动 HTTP 服务器
   - 优雅关闭处理
   包含基础的信号处理
   "
   ```

### Cursor AI 配合任务
- 使用 Cursor Chat 解决编译错误
- 优化 CMake 配置
- 完善错误处理逻辑
- 添加基础单元测试

## 阶段 2：决策引擎 (2-3 天)

### 目标
- 实现规则引擎和模式匹配
- 集成决策逻辑
- 达到基础功能完整性

### Claude Code 任务序列

6. **生成规则引擎**
   ```bash
   claude-code create src/engine/rule_engine.cpp include/engine/rule_engine.hpp --prompt "
   使用 ExprTk 创建规则引擎：
   - 支持动态加载规则表达式
   - 规则编译和缓存
   - 多线程安全执行
   - 规则优先级和权重
   - 性能监控（执行时间、命中率）
   "
   ```

7. **生成模式匹配器**
   ```bash
   claude-code create src/engine/pattern_matcher.cpp include/engine/pattern_matcher.hpp --prompt "
   使用 Hyperscan 创建模式匹配器：
   - 正则表达式预编译
   - 批量匹配支持
   - 黑白名单检查
   - 性能优化（SIMD）
   包含模式数据库管理
   "
   ```

8. **生成决策融合模块**
   ```bash
   claude-code create src/engine/decision_fusion.cpp include/engine/decision_fusion.hpp --prompt "
   创建决策融合模块：
   - 多个决策源的结果合并
   - 加权投票算法
   - 规则优先级处理
   - 置信度计算
   - 决策原因跟踪
   "
   ```

### Cursor AI 配合任务
- 调试规则表达式解析
- 优化模式匹配性能
- 完善决策逻辑
- 创建规则配置文件

## 阶段 3：特征和缓存 (2-3 天)

### 目标
- 实现多级缓存系统
- 特征提取和管理
- 性能优化

### Claude Code 任务序列

9. **生成特征提取器**
   ```bash
   claude-code create src/feature/extractor.cpp include/feature/extractor.hpp --prompt "
   创建特征提取器：
   - 从 Transaction 提取数值特征
   - 时序特征计算
   - 聚合特征生成
   - 特征标准化和编码
   支持特征版本管理
   "
   ```

10. **生成多级缓存**
    ```bash
    claude-code create src/feature/cache.cpp include/feature/cache.hpp --prompt "
    使用 parallel-hashmap 创建多级缓存：
    - L1: thread_local 缓存
    - L2: 进程级 LRU 缓存  
    - L3: Redis 缓存（可选）
    - 缓存穿透保护
    - 性能指标收集
    "
    ```

### Cursor AI 配合任务
- 优化缓存命中率
- 调整缓存大小和 TTL
- 实现缓存预热
- 性能测试和调优

## 阶段 4：ML 推理 (2-3 天)

### 目标
- 集成 ONNX Runtime
- 模型管理和推理
- 性能优化

### Claude Code 任务序列

11. **生成模型管理器**
    ```bash
    claude-code create src/inference/model_manager.cpp include/inference/model_manager.hpp --prompt "
    使用 ONNX Runtime 创建模型管理器：
    - 模型加载和版本管理
    - 多模型支持
    - 模型热更新
    - 内存管理优化
    - A/B 测试支持
    "
    ```

12. **生成推理预测器**
    ```bash
    claude-code create src/inference/predictor.cpp include/inference/predictor.hpp --prompt "
    创建 ML 推理预测器：
    - 批量推理支持
    - 特征向量预处理
    - 推理结果后处理
    - 性能监控
    - 异常处理和降级
    "
    ```

### Cursor AI 配合任务
- 优化推理性能
- 实现模型降级机制
- 添加推理监控
- 创建模型测试用例

## 阶段 5：监控和部署 (1-2 天)

### 目标
- 完善监控系统
- 部署脚本
- 性能测试

### Claude Code 任务序列

13. **生成监控系统**
    ```bash
    claude-code create src/monitor/metrics.cpp include/monitor/metrics.hpp --prompt "
    使用 prometheus-cpp 创建监控系统：
    - 延迟直方图
    - 吞吐量计数器
    - 错误率统计
    - 缓存命中率
    - 自定义业务指标
    "
    ```

14. **生成性能测试**
    ```bash
    claude-code create tests/benchmark/load_test.cpp --prompt "
    创建性能基准测试：
    - 并发请求测试
    - 延迟分布测试  
    - 吞吐量测试
    - 内存使用监控
    - SLO 验证脚本
    "
    ```

### Cursor AI 配合任务
- 优化编译配置
- 创建部署脚本
- 性能调优
- 文档完善

## 实施建议

### 使用 Claude Code 的最佳实践

1. **具体化提示**：每次提示都要包含具体的技术要求
2. **增量开发**：先实现基础功能，再逐步完善
3. **依赖管理**：明确指定使用的第三方库版本
4. **错误处理**：要求生成完整的异常处理代码

### 使用 Cursor AI 的最佳实践

1. **代码审查**：使用 Cursor Chat 审查生成的代码
2. **调试助手**：遇到编译错误时使用 Cursor 解决
3. **性能优化**：使用 Cursor 建议性能改进点
4. **测试生成**：使用 Cursor 生成单元测试

### 质量控制

- **每个阶段都要编译测试**
- **使用 sanitizer 检查内存安全**
- **定期运行性能基准测试**
- **保持代码风格一致性**

### 可能遇到的挑战

1. **依赖库安装**：Hyperscan、ONNX Runtime 等可能需要手动编译
2. **性能调优**：可能需要多轮迭代才能达到 SLO 目标
3. **内存管理**：C++ 需要仔细管理内存生命周期
4. **并发安全**：多线程代码需要仔细设计锁机制

准备好开始了吗？建议先运行初始化脚本，然后从阶段 1 开始逐步实施。