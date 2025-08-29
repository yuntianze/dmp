#!/bin/bash

# DMP 项目依赖安装脚本 - Apple Silicon (M3 Max) 优化版本
# 将所有第三方依赖安装到 third_party/ 目录

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印状态函数
print_status() {
    local status=$1
    local message=$2
    case $status in
        "success")
            echo -e "${GREEN}✅${NC} $message"
            ;;
        "error")
            echo -e "${RED}❌${NC} $message"
            ;;
        "warning")
            echo -e "${YELLOW}⚠️${NC} $message"
            ;;
        "info")
            echo -e "${BLUE}ℹ️${NC} $message"
            ;;
    esac
}

print_status "info" "DMP 依赖安装脚本 - Apple Silicon (M3 Max) 优化"
print_status "info" "=============================================="

# 检查系统环境
check_system() {
    print_status "info" "检查系统环境..."
    
    # 检查 macOS 版本
    if [[ "$OSTYPE" != "darwin"* ]]; then
        print_status "error" "此脚本仅支持 macOS"
        exit 1
    fi
    
    # 检查架构
    ARCH=$(uname -m)
    if [[ "$ARCH" != "arm64" ]]; then
        print_status "warning" "检测到非 ARM64 架构: $ARCH，某些优化可能不适用"
    else
        print_status "success" "检测到 Apple Silicon (ARM64) 架构"
    fi
    
    # 检查 Homebrew
    if ! command -v brew &> /dev/null; then
        print_status "error" "请先安装 Homebrew: https://brew.sh"
        exit 1
    fi
    
    print_status "success" "系统环境检查完成"
}

# 设置项目目录结构
setup_directories() {
    print_status "info" "设置项目目录结构..."
    
    # 确保在项目根目录
    if [[ ! -f "CMakeLists.txt" ]]; then
        print_status "error" "请在项目根目录运行此脚本"
        exit 1
    fi
    
    # 创建 third_party 目录结构
    mkdir -p third_party/{src,build,install}
    mkdir -p third_party/install/{include,lib,bin,share}
    
    print_status "success" "目录结构创建完成"
}

# 安装系统依赖
install_system_dependencies() {
    print_status "info" "安装系统依赖..."
    
    # 基础开发工具
    brew_packages=(
        "cmake"
        "ninja"
        "pkg-config"
        "git"
        "wget"
        "curl"
    )
    
    for package in "${brew_packages[@]}"; do
        if brew list "$package" &>/dev/null; then
            print_status "success" "$package 已安装"
        else
            print_status "info" "安装 $package..."
            brew install "$package"
        fi
    done
    
    print_status "success" "系统依赖安装完成"
}

# 下载并编译第三方库
build_third_party_library() {
    local name=$1
    local git_url=$2
    local git_tag=$3
    local cmake_options=$4
    
    print_status "info" "构建 $name..."
    
    cd third_party/src
    
    # 下载源码
    if [[ ! -d "$name" ]]; then
        print_status "info" "下载 $name 源码..."
        git clone --depth 1 --branch "$git_tag" "$git_url" "$name"
    else
        print_status "info" "$name 源码已存在，跳过下载"
    fi
    
    # 构建
    cd "$name"
    mkdir -p build
    cd build
    
    print_status "info" "配置 $name..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PWD/../../../install" \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        $cmake_options
    
    print_status "info" "编译 $name..."
    cmake --build . --config Release -j$(sysctl -n hw.ncpu)
    
    print_status "info" "安装 $name..."
    cmake --install .
    
    cd ../../../..
    print_status "success" "$name 构建完成"
}

# 构建所有第三方依赖
build_all_dependencies() {
    print_status "info" "开始构建第三方依赖库..."
    
    # fmt 库 - 使用与spdlog兼容的版本
    build_third_party_library \
        "fmt" \
        "https://github.com/fmtlib/fmt.git" \
        "9.1.0" \
        "-DFMT_DOC=OFF -DFMT_TEST=OFF"
    
    # spdlog 库 - 使用与fmt 9.1.0兼容的版本
    build_third_party_library \
        "spdlog" \
        "https://github.com/gabime/spdlog.git" \
        "v1.12.0" \
        "-DSPDLOG_BUILD_TESTS=OFF -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_FMT_EXTERNAL=ON"
    
    # simdjson 库
    build_third_party_library \
        "simdjson" \
        "https://github.com/simdjson/simdjson.git" \
        "v3.6.0" \
        "-DSIMDJSON_BUILD_STATIC=ON -DSIMDJSON_ENABLE_THREADS=ON"
    
    # parallel-hashmap 库
    build_third_party_library \
        "parallel-hashmap" \
        "https://github.com/greg7mdp/parallel-hashmap.git" \
        "v1.3.11" \
        "-DPHMAP_BUILD_TESTS=OFF -DPHMAP_BUILD_EXAMPLES=OFF"
    
    # toml++ 库
    build_third_party_library \
        "tomlplusplus" \
        "https://github.com/marzer/tomlplusplus.git" \
        "v3.4.0" \
        "-DTOMLPP_BUILD_TESTS=OFF -DTOMLPP_BUILD_EXAMPLES=OFF"
    
    # prometheus-cpp 库
    build_third_party_library \
        "prometheus-cpp" \
        "https://github.com/jupp0r/prometheus-cpp.git" \
        "v1.2.4" \
        "-DENABLE_TESTING=OFF -DENABLE_PUSH=OFF -DUSE_THIRDPARTY_LIBRARIES=OFF"
    
    # Drogon 框架
    build_third_party_library \
        "drogon" \
        "https://github.com/drogonframework/drogon.git" \
        "v1.9.1" \
        "-DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF"
    
    print_status "success" "所有基础依赖构建完成"
}

# 安装 ONNX Runtime (预编译版本)
install_onnx_runtime() {
    print_status "info" "安装 ONNX Runtime..."
    
    cd third_party
    
    # ONNX Runtime 版本
    ONNX_VERSION="1.17.0"
    ONNX_PLATFORM="osx-arm64"
    ONNX_PACKAGE="onnxruntime-${ONNX_PLATFORM}-${ONNX_VERSION}"
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/${ONNX_PACKAGE}.tgz"
    
    if [[ ! -d "onnxruntime" ]]; then
        print_status "info" "下载 ONNX Runtime ${ONNX_VERSION}..."
        wget -O "${ONNX_PACKAGE}.tgz" "$ONNX_URL"
        
        print_status "info" "解压 ONNX Runtime..."
        tar -xzf "${ONNX_PACKAGE}.tgz"
        mv "$ONNX_PACKAGE" onnxruntime
        rm "${ONNX_PACKAGE}.tgz"
        
        # 复制到 install 目录
        cp -r onnxruntime/include/* install/include/
        cp -r onnxruntime/lib/* install/lib/
        
        print_status "success" "ONNX Runtime 安装完成"
    else
        print_status "success" "ONNX Runtime 已存在"
    fi
    
    cd ..
}

# 安装 Hyperscan (特殊处理 - Apple Silicon 支持)
install_hyperscan() {
    print_status "warning" "Hyperscan 在 Apple Silicon 上的支持有限，使用 vectorscan 作为替代..."
    
    # 使用 Homebrew 安装 vectorscan (Hyperscan 的 ARM64 兼容版本)
    if brew list vectorscan &>/dev/null; then
        print_status "success" "vectorscan 已安装"
    else
        print_status "info" "安装 vectorscan..."
        brew install vectorscan
    fi
    
    # 创建符号链接到 third_party
    mkdir -p third_party/install/include/hs
    mkdir -p third_party/install/lib
    
    # 链接头文件
    if [[ -d "/opt/homebrew/include/hs" ]]; then
        ln -sf /opt/homebrew/include/hs/* third_party/install/include/hs/
    fi
    
    # 链接库文件
    if [[ -f "/opt/homebrew/lib/libhs.dylib" ]]; then
        ln -sf /opt/homebrew/lib/libhs.dylib third_party/install/lib/
    fi
    
    print_status "success" "Hyperscan/Vectorscan 配置完成"
}

# 创建环境配置文件
create_env_config() {
    print_status "info" "创建环境配置文件..."
    
    cat > setup_env.sh << 'EOF'
#!/bin/bash

# DMP 项目环境配置 - 使用 third_party 依赖

# 设置第三方库路径
export DMP_THIRD_PARTY_ROOT="$(pwd)/third_party/install"

# 设置 CMAKE 路径
export CMAKE_PREFIX_PATH="$DMP_THIRD_PARTY_ROOT:$CMAKE_PREFIX_PATH"

# 设置库路径
export LD_LIBRARY_PATH="$DMP_THIRD_PARTY_ROOT/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$DMP_THIRD_PARTY_ROOT/lib:$DYLD_LIBRARY_PATH"

# 设置包含路径
export CPLUS_INCLUDE_PATH="$DMP_THIRD_PARTY_ROOT/include:$CPLUS_INCLUDE_PATH"

# 设置 PKG_CONFIG 路径
export PKG_CONFIG_PATH="$DMP_THIRD_PARTY_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH"

echo "✅ DMP 环境配置已加载"
echo "📁 第三方库路径: $DMP_THIRD_PARTY_ROOT"
EOF
    
    chmod +x setup_env.sh
    print_status "success" "环境配置文件创建完成: setup_env.sh"
}

# 更新 CMakeLists.txt
update_cmake_config() {
    print_status "info" "更新 CMake 配置以使用 third_party 依赖..."
    
    # 备份原始文件
    cp CMakeLists.txt CMakeLists.txt.backup
    
    print_status "success" "CMake 配置将通过单独的脚本更新"
}

# 验证安装
verify_installation() {
    print_status "info" "验证安装..."
    
    # 检查关键文件
    local required_files=(
        "third_party/install/include/fmt/core.h"
        "third_party/install/lib/libfmt.a"
        "third_party/install/include/spdlog/spdlog.h"
        "third_party/install/include/simdjson.h"
        "third_party/install/include/onnxruntime_cxx_api.h"
    )
    
    local missing_files=()
    
    for file in "${required_files[@]}"; do
        if [[ -f "$file" ]]; then
            print_status "success" "✓ $file"
        else
            print_status "error" "✗ $file"
            missing_files+=("$file")
        fi
    done
    
    if [[ ${#missing_files[@]} -eq 0 ]]; then
        print_status "success" "所有依赖安装验证成功！"
        return 0
    else
        print_status "error" "以下文件缺失: ${missing_files[*]}"
        return 1
    fi
}

# 主函数
main() {
    print_status "info" "开始 DMP 依赖安装流程..."
    
    check_system
    setup_directories
    install_system_dependencies
    build_all_dependencies
    install_onnx_runtime
    install_hyperscan
    create_env_config
    update_cmake_config
    
    if verify_installation; then
        print_status "success" "🎉 DMP 依赖安装完成！"
        print_status "info" "下一步:"
        print_status "info" "1. 运行: source setup_env.sh"
        print_status "info" "2. 运行: ./scripts/build.sh"
        print_status "info" "3. 测试: ./build_release/dmp_server"
    else
        print_status "error" "安装过程中出现错误，请检查日志"
        exit 1
    fi
}

# 如果直接运行脚本
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
