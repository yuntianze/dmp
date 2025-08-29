# DMP 风控系统 - 安装指南

## 🚀 快速开始

### 系统要求

- **操作系统**: macOS (Apple Silicon) / Linux
- **编译器**: clang++ 15.0+ 或 g++ 11.0+
- **CMake**: 3.20+
- **C++ 标准**: C++20

### 环境准备

#### macOS (推荐)
```bash
# 安装基础工具
brew install cmake wget ragel boost

# 确保使用最新的 Xcode Command Line Tools
xcode-select --install
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install cmake g++ wget ragel libboost-all-dev

# CentOS/RHEL
sudo yum install cmake gcc-c++ wget ragel boost-devel
```

### 安装步骤

#### 1. 克隆项目
```bash
git clone https://github.com/yuntianze/dmp.git
cd dmp
```

#### 2. 安装第三方依赖
```bash
# 使用提供的安装脚本
./scripts/setup_dependencies.sh

# 或者手动运行macOS专用脚本
./scripts/macos_setup.sh
```

#### 3. 配置环境
```bash
# 加载环境变量
source setup_env.sh
```

#### 4. 构建项目
```bash
# 方法1：使用构建脚本
./scripts/setup_and_build.sh

# 方法2：手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### 5. 运行测试
```bash
# 运行所有测试
./scripts/run_tests.sh

# 或手动运行
cd build
ctest --verbose
```

### 依赖库说明

项目使用以下第三方库，所有库都安装到 `third_party/install/`:

| 库名称 | 版本 | 用途 |
|--------|------|------|
| **spdlog** | 1.15.3 | 高性能日志系统 |
| **simdjson** | 3.6.0 | 高速JSON解析 |
| **toml++** | 3.4.0 | TOML配置文件解析 |
| **parallel-hashmap** | 1.3.11 | 高性能哈希表 |
| **ONNX Runtime** | 1.17.0 | ML模型推理引擎 |
| **Hyperscan/Vectorscan** | latest | 正则表达式匹配 |
| **GoogleTest** | 1.14.0 | 单元测试框架 |

### 目录结构

```
dmp/
├── build/                  # 构建目录 (生成)
├── cmake/                  # CMake 配置文件
├── config/                 # 配置文件
│   ├── server.toml        # 服务器配置
│   ├── logging.toml       # 日志配置
│   ├── features.yaml      # 特征工程配置
│   ├── models.toml        # ML模型配置
│   └── rules.json         # 规则引擎配置
├── docs/                   # 文档
├── include/                # 头文件
│   ├── common/            # 通用类型和配置
│   ├── core/              # 核心业务逻辑
│   └── utils/             # 工具类
├── logs/                   # 日志输出目录 (生成)
├── scripts/                # 构建和部署脚本
├── src/                    # 源代码
│   ├── common/            # 通用功能实现
│   ├── core/              # 核心业务实现
│   ├── server/            # HTTP服务器
│   ├── utils/             # 工具类实现
│   └── main.cpp           # 主程序入口
├── tests/                  # 单元测试
├── third_party/            # 第三方依赖 (生成)
└── setup_env.sh           # 环境配置脚本
```

### 配置说明

#### 服务器配置 (config/server.toml)
```toml
[server]
host = "0.0.0.0"
port = 8080
threads = 8

[performance]
target_p99_ms = 50
target_qps = 10000
```

#### 日志配置 (config/logging.toml)
```toml
[logging]
level = "INFO"
pattern = "[%Y-%m-%d %H:%M:%S.%f] [%l] [%s:%#] [%!] %v"

[sinks.file]
enabled = true
filename = "logs/dmp_server.log"
max_size_mb = 100
max_files = 10
```

### 运行应用

```bash
# 进入构建目录
cd build

# 运行主程序（指定配置文件路径）
./dmp_server ../config/server.toml

# 运行特定测试
./test_config
./test_transaction
./test_handlers
./test_metrics
```

### 日志查看

```bash
# 查看主日志
tail -f logs/dmp_server.log

# 查看错误日志
tail -f logs/dmp_error.log

# 查看性能日志
tail -f logs/dmp_performance.log

# 查看审计日志
tail -f logs/dmp_audit_*.log
```

### 故障排除

#### 1. 编译错误
```bash
# 清理并重新构建
rm -rf build third_party
./scripts/setup_dependencies.sh
./scripts/setup_and_build.sh
```

#### 2. 依赖问题
```bash
# 检查环境
source setup_env.sh
echo $DMP_THIRD_PARTY_ROOT

# 验证库文件
ls -la third_party/install/lib/
```

#### 3. 权限问题
```bash
# 修复脚本权限
chmod +x scripts/*.sh
chmod +x setup_env.sh
```

#### 4. Apple Silicon 特定问题
```bash
# 强制使用ARM64架构
export CMAKE_OSX_ARCHITECTURES=arm64
export ARCHFLAGS="-arch arm64"
```

### 性能调优

#### 1. 编译优化
```bash
# Release 模式构建
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### 2. 日志级别调整
```bash
# 生产环境建议使用 ERROR 级别
export SPDLOG_LEVEL=error
```

#### 3. 系统配置
```bash
# 增加文件描述符限制
ulimit -n 65536

# 优化网络参数 (Linux)
echo 'net.core.somaxconn = 65536' >> /etc/sysctl.conf
```

### 开发工具

#### 1. 代码格式化
```bash
# 使用 clang-format (需要安装)
find src include -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

#### 2. 静态分析
```bash
# 使用 clang-tidy (需要安装)
clang-tidy src/**/*.cpp -- -I include -I third_party/install/include
```

#### 3. 性能分析
```bash
# 使用 valgrind (Linux)
valgrind --tool=callgrind ./dmp_server config/server.toml

# 使用 Instruments (macOS)
instruments -t "Time Profiler" ./dmp_server config/server.toml
```

## 🤝 贡献指南

1. Fork 本仓库
2. 创建特性分支: `git checkout -b feature/amazing-feature`
3. 提交更改: `git commit -m 'Add amazing feature'`
4. 推送分支: `git push origin feature/amazing-feature`
5. 提交 Pull Request

## 📄 许可证

本项目使用 [MIT License](LICENSE)。
