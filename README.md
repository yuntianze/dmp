# DMP 风控原型系统

基于 C++ 的高性能实时风控决策引擎原型项目。

## 🎯 性能目标

- **P99 延迟**: ≤ 50ms  
- **吞吐量**: ≥ 10,000 TPS
- **可用性**: 99.9%
- **内存使用**: < 4GB
- **CPU 使用率**: < 80%

## 🏗️ 技术栈

| 组件 | 技术选型 | 版本 | 说明 |
|-----|---------|------|------|
| HTTP 服务器 | Drogon | 1.9.x | 高性能协程框架 |
| JSON 解析 | simdjson | 3.6.0 | SIMD 加速解析 |
| 规则引擎 | ExprTk | 0.0.2 | JIT 编译表达式 |
| 模式匹配 | Hyperscan | 5.4.x | Intel 正则引擎 |
| ML 推理 | ONNX Runtime | 1.17.x | 跨平台优化 |
| 缓存 | parallel-hashmap | 1.3.11 | 高性能哈希表 |
| 日志 | spdlog | 1.13.x | 异步日志 |
| 监控 | prometheus-cpp | 1.2.x | 指标收集 |

## 🚀 快速开始

### 环境要求

- **操作系统**: Linux (Ubuntu 20.04+) / macOS (10.15+)
- **编译器**: GCC 10+ 或 Clang 12+
- **CMake**: 3.20+
- **内存**: 8GB+
- **CPU**: 支持 AVX2 的 x64 处理器

### 构建

```bash
# 克隆项目
git clone <your-repo-url>
cd dmp

# 安装依赖（Ubuntu）
sudo apt-get install -y \
    build-essential cmake \
    libdrogon-dev libspdlog-dev libfmt-dev \
    libhyperscan-dev libprometheus-cpp-dev

# 构建项目
./scripts/build.sh Release

# 运行服务
./build_release/dmp_server
```

### macOS 构建

```bash
# 安装依赖
brew install cmake drogon spdlog fmt hyperscan

# 构建
./scripts/build.sh Release
```

## 📊 API 接口

### 风控决策 API

```bash
POST /api/v1/decision
Content-Type: application/json

{
  "request_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": 1703001234567,
  "transaction": {
    "amount": 1299.99,
    "currency": "USD",
    "merchant_id": "MERCH_12345",
    "merchant_category": "5411",
    "pos_entry_mode": "CHIP"
  },
  "card": {
    "token": "tok_4242424242424242",
    "issuer_country": "US",
    "card_brand": "VISA"
  },
  "device": {
    "ip": "192.168.1.1",
    "fingerprint": "df_abc123",
    "user_agent": "Mozilla/5.0..."
  },
  "customer": {
    "id": "cust_789",
    "risk_score": 35,
    "account_age_days": 365
  }
}
```

### 响应格式

```json
{
  "request_id": "550e8400-e29b-41d4-a716-446655440000",
  "decision": "APPROVE",
  "risk_score": 15.5,
  "reasons": [
    {
      "code": "RULE_001",
      "description": "Transaction amount within normal range",
      "score_impact": -5.0
    }
  ],
  "latency_ms": 12.3,
  "model_version": "v2024.01.15",
  "timestamp": 1703001234579
}
```

### 健康检查

```bash
GET /health     # 服务健康状态
GET /ready      # 服务就绪状态
```

## 🔧 配置

配置文件位于 `config/` 目录：

- `server.toml` - 服务器配置
- `rules.json` - 规则配置  
- `features.yaml` - 特征配置
- `models.toml` - 模型配置

## 🧪 性能测试

```bash
# 基准测试
python3 scripts/benchmark.py --requests 10000 --concurrency 100

# 压力测试
python3 scripts/benchmark.py --requests 100000 --concurrency 500
```

## 📈 监控指标

访问 http://localhost:9090/metrics 查看 Prometheus 指标：

- `dmp_request_duration_seconds` - 请求延迟分布
- `dmp_requests_total` - 总请求数
- `dmp_decisions_total` - 决策分布
- `dmp_cache_hit_rate` - 缓存命中率

## 🏗️ 项目结构

```
dmp/
├── include/           # 头文件
│   ├── common/       # 公共定义
│   ├── core/         # 核心类型
│   └── utils/        # 工具类
├── src/              # 源代码
│   ├── server/       # HTTP 服务器
│   ├── engine/       # 决策引擎
│   ├── feature/      # 特征工程
│   ├── inference/    # ML 推理
│   └── monitor/      # 监控
├── config/           # 配置文件
├── models/           # ML 模型
├── data/             # 数据文件
├── scripts/          # 脚本
└── docs/             # 文档
```

## 🚀 部署

```bash
# 自动化部署
sudo ./scripts/deploy.sh

# 手动部署
cmake --install build_release --prefix /usr/local
sudo systemctl start dmp
```

## 📝 开发指南

1. **性能优化**: 重点关注热路径优化，使用 SIMD、缓存友好的数据结构
2. **内存管理**: 避免动态分配，使用对象池和内存池
3. **并发安全**: 使用无锁数据结构，避免锁竞争
4. **监控告警**: 关键指标达到阈值时及时告警

## 📊 基准测试结果

| 指标 | 目标值 | 当前值 | 状态 |
|-----|--------|--------|------|
| P50 延迟 | ≤ 10ms | - | 🔄 |
| P95 延迟 | ≤ 30ms | - | 🔄 |
| P99 延迟 | ≤ 50ms | - | 🔄 |
| QPS | ≥ 10,000 | - | 🔄 |

## 🤝 贡献

1. Fork 项目
2. 创建特性分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 📄 许可证

[MIT License](LICENSE)
