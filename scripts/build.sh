#!/bin/bash
set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build_$(echo $BUILD_TYPE | tr '[:upper:]' '[:lower:]')"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "🔨 构建类型: $BUILD_TYPE"
echo "📁 构建目录: $BUILD_DIR"
echo "⚡ 并行度: $CORES 核心"

# 创建构建目录
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 配置 CMake
echo "🔧 配置项目..."

# 检查并使用 GNU GCC（如果可用且环境变量设置了）
if command -v gcc-14 &> /dev/null && command -v g++-14 &> /dev/null && [ "$CC" = "gcc-14" ]; then
    echo "🎯 使用 GNU GCC 14 进行编译..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_C_COMPILER=gcc-14 \
        -DCMAKE_CXX_COMPILER=g++-14 \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_VERBOSE_MAKEFILE=OFF
else
    echo "🔧 使用系统默认编译器..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_VERBOSE_MAKEFILE=OFF
fi

# 编译
echo "🚀 开始编译..."
cmake --build . --parallel $CORES

# 运行测试（如果是 Debug 模式）
if [ "$BUILD_TYPE" = "Debug" ]; then
    echo "🧪 运行单元测试..."
    ctest --parallel $CORES --output-on-failure || true
fi

echo "✅ 构建完成！"
echo "📍 可执行文件位于: $BUILD_DIR/dmp_server"
echo "🏃 运行命令: ./$BUILD_DIR/dmp_server"
