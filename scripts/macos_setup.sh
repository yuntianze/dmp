#!/bin/bash

# macOS DMP 项目环境配置脚本
set -e

echo "🍎 配置 macOS 开发环境..."

# 检查是否安装了 Homebrew
if ! command -v brew &> /dev/null; then
    echo "安装 Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

echo "📦 安装基础开发工具..."

# 安装 Xcode Command Line Tools
xcode-select --install 2>/dev/null || true

# 安装基础工具
brew install \
    cmake \
    pkg-config \
    llvm \
    boost \
    openssl \
    zlib \
    brotli \
    c-ares \
    jsoncpp \
    curl \
    wget \
    git

echo "🔧 安装 C++ 包管理器..."

# 安装 vcpkg
if [ ! -d "$HOME/vcpkg" ]; then
    cd $HOME
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    
    # 添加到环境变量
    echo 'export PATH="$HOME/vcpkg:$PATH"' >> ~/.zshrc
    echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.zshrc
    source ~/.zshrc
fi

echo "📚 通过 vcpkg 安装 C++ 库..."

cd $HOME/vcpkg

# 安装主要依赖
./vcpkg install \
    drogon[core] \
    spdlog \
    fmt \
    gtest \
    benchmark

echo "⚡ 安装 simdjson..."

# 创建临时目录
TEMP_DIR="/tmp/dmp_deps"
mkdir -p $TEMP_DIR
cd $TEMP_DIR

# 安装 simdjson
git clone https://github.com/simdjson/simdjson.git
cd simdjson
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
    -DBUILD_SHARED_LIBS=ON

make -j$(sysctl -n hw.ncpu)
sudo make install

echo "🗂️ 安装 parallel-hashmap..."

cd $TEMP_DIR
git clone https://github.com/greg7mdp/parallel-hashmap.git
cd parallel-hashmap
mkdir build && cd build

cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo make install

echo "🔍 安装 Hyperscan (Intel 版本)..."

# 检查处理器架构
ARCH=$(uname -m)
if [[ "$ARCH" == "arm64" ]]; then
    echo "⚠️ 注意：Hyperscan 在 Apple Silicon 上可能需要特殊配置"
    echo "建议使用 Rosetta 2 或者替代方案"
    
    # 安装 Rosetta 2 版本
    arch -x86_64 brew install hyperscan || {
        echo "❌ Hyperscan 安装失败，将使用替代方案"
        echo "可以考虑使用 std::regex 或 RE2 库"
    }
else
    # Intel Mac
    brew install hyperscan
fi

echo "🤖 安装 ONNX Runtime..."

# 下载适合 macOS 的预编译版本
ONNX_VERSION="1.17.0"
if [[ "$ARCH" == "arm64" ]]; then
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-osx-arm64-${ONNX_VERSION}.tgz"
else
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-osx-x86_64-${ONNX_VERSION}.tgz"
fi

cd $TEMP_DIR
curl -L $ONNX_URL -o onnxruntime.tgz
tar -xzf onnxruntime.tgz

# 找到解压后的目录
ONNX_DIR=$(find . -name "onnxruntime-osx-*" -type d | head -1)

# 安装到系统目录
sudo cp -r $ONNX_DIR/include/* /usr/local/include/
sudo cp -r $ONNX_DIR/lib/* /usr/local/lib/

echo "📊 安装 ExprTk..."

cd $TEMP_DIR
git clone https://github.com/ArashPartow/exprtk.git
sudo mkdir -p /usr/local/include/exprtk
sudo cp exprtk/exprtk.hpp /usr/local/include/exprtk/

echo "🧵 安装 BS::thread_pool..."

cd $TEMP_DIR
git clone https://github.com/bshoshany/thread-pool.git
sudo mkdir -p /usr/local/include/BS
sudo cp thread-pool/BS_thread_pool.hpp /usr/local/include/BS/

echo "🔧 配置 CMake 环境..."

# 设置 CMake 环境变量
cat >> ~/.zshrc << 'EOF'

# DMP 项目环境变量
export CMAKE_PREFIX_PATH="/usr/local:$HOME/vcpkg/installed/$(uname -m)-osx:$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export DYLD_LIBRARY_PATH="/usr/local/lib:$DYLD_LIBRARY_PATH"

# LLVM 配置 (for better C++20 support)
export CC=/usr/local/bin/clang
export CXX=/usr/local/bin/clang++
EOF

echo "📝 创建 macOS 特定的 CMake 配置..."

# 回到项目目录
cd - > /dev/null

# 创建 macOS 特定的 CMake 工具链文件
cat > cmake/macOS-toolchain.cmake << 'EOF'
# macOS 工具链配置

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})

# 编译器设置
set(CMAKE_C_COMPILER /usr/local/bin/clang)
set(CMAKE_CXX_COMPILER /usr/local/bin/clang++)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS 特定编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")

# 架构特定优化
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mcpu=apple-m1")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
endif()

# 库搜索路径
set(CMAKE_PREFIX_PATH 
    /usr/local
    $ENV{HOME}/vcpkg/installed/${CMAKE_SYSTEM_PROCESSOR}-osx
    ${CMAKE_PREFIX_PATH}
)
EOF

# 创建 macOS 构建脚本
cat > scripts/build_macos.sh << 'EOF'
#!/bin/bash
set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build_${BUILD_TYPE,,}_macos"

echo "🍎 macOS 构建 - 类型: $BUILD_TYPE"
echo "📂 构建目录: $BUILD_DIR"

# 确保环境变量加载
source ~/.zshrc 2>/dev/null || true

# 创建构建目录
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 配置 CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/macOS-toolchain.cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DVCPKG_TARGET_TRIPLET="$(uname -m)-osx"

# 编译
cmake --build . --parallel $(sysctl -n hw.ncpu)

echo "✅ 构建完成！"
echo "🚀 可执行文件: $BUILD_DIR/dmp_server"
EOF

chmod +x scripts/build_macos.sh

echo "🧹 清理临时文件..."
rm -rf $TEMP_DIR

echo ""
echo "✅ macOS 开发环境配置完成！"
echo ""
echo "📋 下一步操作："
echo "1. 重新加载环境变量: source ~/.zshrc"
echo "2. 使用 macOS 构建脚本: ./scripts/build_macos.sh"
echo "3. 在 Cursor 中开始开发: cursor ."
echo ""
echo "⚠️ 注意事项："
echo "- 如果使用 Apple Silicon，某些库可能需要 Rosetta 2"
echo "- 建议使用 LLVM 的 clang++ 而不是 Xcode 的版本"
echo "- 性能测试时注意 Apple Silicon 的架构差异"