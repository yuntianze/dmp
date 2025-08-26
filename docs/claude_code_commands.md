# Claude Code 实施步骤

运行上面的初始化脚本后，按以下顺序使用 Claude Code 生成代码：

## 1. 生成核心数据结构

```bash
claude-code --prompt "基于 DMP 设计文档，生成 include/common/types.hpp，包含 Transaction 结构体和所有核心数据类型定义，使用现代 C++20 特性"
```

## 2. 生成配置管理

```bash
claude-code --prompt "生成 include/common/config.hpp 和对应的实现，使用 toml++ 库解析 TOML 配置文件，包含所有系统配置选项"
```

## 3. 生成 HTTP 服务器

```bash
claude-code --prompt "生成基于 Drogon 框架的 HTTP 服务器代码 src/server/server.cpp 和 src/server/handlers.cpp，实现风控 API 端点，支持 JSON 请求响应"
```

## 4. 生成规则引擎

```bash
claude-code --prompt "生成 ExprTk 规则引擎 src/engine/rule_engine.cpp，支持动态规则加载、JIT 编译和并发执行"
```

## 5. 生成模式匹配器

```bash
claude-code --prompt "生成基于 Hyperscan 的模式匹配器 src/engine/pattern_matcher.cpp，支持正则表达式批量匹配和名单检查"
```

## 6. 生成特征缓存

```bash
claude-code --prompt "生成多级特征缓存系统 src/feature/cache.cpp，使用 parallel-hashmap，实现 L1/L2/L3 缓存架构"
```

## 7. 生成 ML 推理模块

```bash
claude-code --prompt "生成 ONNX Runtime ML 推理模块 src/inference/model_manager.cpp 和 predictor.cpp，支持模型热加载和批量推理"
```

## 8. 生成监控系统

```bash
claude-code --prompt "生成基于 Prometheus 的监控系统 src/monitor/metrics.cpp，包含所有性能指标收集"
```

## 9. 生成主函数

```bash
claude-code --prompt "生成 src/main.cpp 主函数，整合所有组件，实现完整的风控决策流程"
```

## 10. 生成构建配置

```bash
claude-code --prompt "生成完整的 CMake 配置，包括所有第三方库的查找和链接，支持性能优化编译选项"
```

## 使用建议

1. **逐个生成**：按顺序生成每个组件，不要一次性生成所有代码
2. **检查依赖**：生成每个组件后，检查其依赖是否正确
3. **测试编译**：生成几个核心组件后就尝试编译，及早发现问题
4. **性能验证**：实现基础功能后，运行性能测试验证 SLO 目标

## 与 Cursor AI 配合

- 使用 Claude Code 生成核心逻辑框架
- 使用 Cursor AI 进行代码细节完善和调试
- 使用 Cursor AI 的 Chat 功能解决编译错误
- 使用 Cursor AI 优化代码性能和结构