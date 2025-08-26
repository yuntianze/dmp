#!/bin/bash
set -e

echo "🚀 DMP 系统部署脚本"

# 安装系统依赖
install_dependencies() {
    echo "📦 检查和安装系统依赖..."
    
    if command -v apt-get >/dev/null 2>&1; then
        # Ubuntu/Debian
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            libdrogon-dev \
            libspdlog-dev \
            libfmt-dev \
            libhyperscan-dev \
            libprometheus-cpp-dev
    elif command -v brew >/dev/null 2>&1; then
        # macOS
        brew install \
            cmake \
            drogon \
            spdlog \
            fmt \
            hyperscan
    else
        echo "❌ 不支持的系统，请手动安装依赖"
        exit 1
    fi
}

# 构建项目
build_project() {
    echo "🔨 构建项目..."
    ./build.sh Release
}

# 安装服务
install_service() {
    echo "📋 安装服务..."
    sudo cmake --install build_release --prefix /usr/local
    
    # 创建服务用户
    sudo useradd -r -s /bin/false dmp || true
    
    # 创建日志目录
    sudo mkdir -p /var/log/dmp
    sudo chown dmp:dmp /var/log/dmp
    
    # 创建数据目录
    sudo mkdir -p /var/lib/dmp
    sudo chown dmp:dmp /var/lib/dmp
}

# 主流程
main() {
    install_dependencies
    build_project
    install_service
    
    echo "✅ 部署完成！"
    echo "🔧 配置文件位于: /usr/local/etc/dmp/"
    echo "📊 启动服务: sudo systemctl start dmp"
}

main "$@"
