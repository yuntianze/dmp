#!/bin/bash

# DMP 高性能风控项目环境设置脚本
# 基于设计文档的性能优化配置
echo "🚀 设置 DMP 高性能风控开发环境..."
echo "📋 目标: P99 ≤ 50ms, 吞吐量 ≥ 10,000 TPS"

# ============================================================================
# 编译器配置
# ============================================================================

# 设置 GNU GCC 环境变量（优先选择）
if command -v gcc-14 &> /dev/null && command -v g++-14 &> /dev/null; then
    export CC=gcc-14
    export CXX=g++-14
    echo "🎯 使用 GNU GCC 14 (高性能优化)"
elif command -v gcc &> /dev/null && command -v g++ &> /dev/null; then
    export CC=gcc
    export CXX=g++
    echo "🔧 使用系统默认编译器"
    # 检查是否为 Apple Clang
    if $CXX --version | grep -q "Apple clang"; then
        echo "⚠️  当前为 Apple Clang，建议安装 GNU GCC: brew install gcc"
    fi
else
    echo "❌ 未找到 C++ 编译器"
    exit 1
fi

export PATH="/usr/local/bin:$PATH"

# ============================================================================
# 性能优化环境变量（按设计文档）
# ============================================================================

# CPU 架构检测和优化
ARCH=$(uname -m)
echo "🏗️ 系统架构: $ARCH"

if [[ "$ARCH" == "arm64" ]]; then
    # Apple Silicon 优化（避免与 GNU GCC 冲突）
    export CFLAGS="-march=native -mtune=native"
    export CXXFLAGS="-march=native -mtune=native"
    echo "🍎 启用 Apple Silicon (M1/M2) 优化"
elif [[ "$ARCH" == "x86_64" ]]; then
    # x86_64 优化
    export CFLAGS="-march=native -mtune=native -mavx2"
    export CXXFLAGS="-march=native -mtune=native -mavx2"
    echo "⚡ 启用 x86_64 + AVX2 优化"
fi

# OpenMP 支持（并行计算）
export OMP_NUM_THREADS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
echo "🔄 OpenMP 线程数: $OMP_NUM_THREADS"

# 内存优化
export MALLOC_ARENA_MAX=4  # 减少内存碎片
export MALLOC_MMAP_THRESHOLD=131072  # 128KB

# C++ 特定优化
export CXXFLAGS="$CXXFLAGS -std=c++20 -ffast-math -funroll-loops"

# ============================================================================
# DMP 项目特定配置
# ============================================================================

# 构建类型（默认 Release）
export CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release}

# 启用 LTO（链接时优化）
export CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

# 并行构建
export CMAKE_BUILD_PARALLEL_LEVEL=$OMP_NUM_THREADS

# ============================================================================
# 验证和报告
# ============================================================================

echo ""
echo "✅ 编译器环境:"
echo "   CC=$CC"
echo "   CXX=$CXX"
echo "   CMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE"

# 显示编译器版本
if command -v $CC &> /dev/null; then
    echo "✅ C 编译器: $($CC --version | head -1)"
fi

if command -v $CXX &> /dev/null; then
    echo "✅ C++ 编译器: $($CXX --version | head -1)"
fi

# 编译器特性检测
echo ""
echo "🔍 编译器特性检测:"
if $CXX -dumpversion &>/dev/null; then
    CXX_VERSION=$($CXX -dumpversion)
    echo "   版本: $CXX_VERSION"
fi

# 检查关键优化标志支持
if $CXX -march=native -E - </dev/null &>/dev/null; then
    echo "   ✅ 支持 -march=native"
fi

if $CXX -mavx2 -E - </dev/null &>/dev/null 2>&1; then
    echo "   ✅ 支持 AVX2 指令集"
fi

if $CXX -std=c++20 -E - </dev/null &>/dev/null; then
    echo "   ✅ 支持 C++20 标准"
fi

echo ""
echo "🎯 DMP 高性能环境设置完成！"
echo "💡 性能优化已启用："
echo "   - 本机架构优化 (-march=native)"
echo "   - 循环展开 (-funroll-loops)"
echo "   - 快速数学 (-ffast-math)"
echo "   - C++20 标准支持"
echo "   - 并行构建 ($CMAKE_BUILD_PARALLEL_LEVEL 线程)"
echo ""
echo "🚀 下一步: 运行 './scripts/build.sh Release' 开始构建"
