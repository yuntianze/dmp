#!/bin/bash

# DMP 项目环境配置 - 修复版本
export DMP_THIRD_PARTY_ROOT="$(pwd)/third_party/install"
export CMAKE_PREFIX_PATH="$DMP_THIRD_PARTY_ROOT:$CMAKE_PREFIX_PATH"
export DYLD_LIBRARY_PATH="$DMP_THIRD_PARTY_ROOT/lib:$DYLD_LIBRARY_PATH"
export CPLUS_INCLUDE_PATH="$DMP_THIRD_PARTY_ROOT/include:$CPLUS_INCLUDE_PATH"
export PKG_CONFIG_PATH="$DMP_THIRD_PARTY_ROOT/lib/pkgconfig:$PKG_CONFIG_PATH"

# 避免系统fmt库冲突
export CMAKE_IGNORE_PATH="/usr/local/include:/usr/local/lib:/opt/homebrew/include:/opt/homebrew/lib"

echo "✅ DMP 环境配置已加载（修复版本）"
echo "📁 第三方库路径: $DMP_THIRD_PARTY_ROOT"
echo "🚫 忽略系统库路径以避免冲突"
