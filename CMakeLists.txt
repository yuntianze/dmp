cmake_minimum_required(VERSION 3.20)

# 强制设置 Apple Silicon (arm64) 架构
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
    set(CMAKE_SYSTEM_PROCESSOR "arm64" CACHE STRING "Target architecture" FORCE)
    set(CMAKE_HOST_SYSTEM_PROCESSOR "arm64" CACHE STRING "Host architecture" FORCE)
    message(STATUS "🍎 强制设置 Apple Silicon (arm64) 架构")
endif()

project(dmp_prototype VERSION 1.0.0 LANGUAGES CXX)

# 重新确认架构设置
if(APPLE)
    set(CMAKE_SYSTEM_PROCESSOR "arm64" CACHE STRING "Target architecture" FORCE)
endif()

# C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



# 编译选项 - Apple Silicon 优化
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined")

# LTO 优化
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# ============================================================================
# Third Party 依赖路径配置 - 修复版本
# ============================================================================

# 设置第三方库根目录
set(THIRD_PARTY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/install")
set(THIRD_PARTY_SRC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src")

# 忽略系统库路径以避免版本冲突
set(CMAKE_IGNORE_PATH "/usr/local/include;/usr/local/lib;/opt/homebrew/include;/opt/homebrew/lib")

# 将第三方库路径添加到 CMAKE_PREFIX_PATH
list(PREPEND CMAKE_PREFIX_PATH "${THIRD_PARTY_ROOT}")

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${THIRD_PARTY_ROOT}/include)
include_directories(${THIRD_PARTY_SRC}/include)  # 包含header-only fmt

# 库目录
link_directories(${THIRD_PARTY_ROOT}/lib)

# ============================================================================
# 基础依赖
# ============================================================================

find_package(Threads REQUIRED)

# ============================================================================
# Third Party 库配置 - 修复版本
# ============================================================================

# fmt 库 - Header-only 模式，避免链接冲突
add_library(fmt INTERFACE)
target_include_directories(fmt INTERFACE "${THIRD_PARTY_SRC}/include")
target_compile_definitions(fmt INTERFACE FMT_HEADER_ONLY=1)

# spdlog 库 - 静态链接
find_library(SPDLOG_LIBRARY 
    NAMES spdlog
    PATHS ${THIRD_PARTY_ROOT}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(spdlog::spdlog UNKNOWN IMPORTED)
set_target_properties(spdlog::spdlog PROPERTIES
    IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/include"
    INTERFACE_COMPILE_DEFINITIONS "SPDLOG_COMPILED_LIB"
)

# simdjson 库
find_library(SIMDJSON_LIBRARY 
    NAMES simdjson
    PATHS ${THIRD_PARTY_ROOT}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(simdjson::simdjson UNKNOWN IMPORTED)
set_target_properties(simdjson::simdjson PROPERTIES
    IMPORTED_LOCATION "${SIMDJSON_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/include"
)

# parallel-hashmap 库 - Header-only
add_library(phmap INTERFACE)
target_include_directories(phmap INTERFACE "${THIRD_PARTY_ROOT}/include")

# toml++ 库 - Header-only
add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE "${THIRD_PARTY_ROOT}/include")

# ============================================================================
# 可选依赖 - ONNX Runtime
# ============================================================================

find_library(ONNXRUNTIME_LIBRARY 
    NAMES onnxruntime
    PATHS ${THIRD_PARTY_ROOT}/lib
    NO_DEFAULT_PATH
)

if(ONNXRUNTIME_LIBRARY AND EXISTS "${THIRD_PARTY_ROOT}/include/onnxruntime_cxx_api.h")
    add_library(ONNXRuntime::onnxruntime UNKNOWN IMPORTED)
    set_target_properties(ONNXRuntime::onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/include"
    )
    set(ENABLE_ML_INFERENCE ON)
    set(ONNXRUNTIME_LIBRARIES ONNXRuntime::onnxruntime)
    message(STATUS "✅ 找到 ONNX Runtime，启用 ML 推理功能: ${ONNXRUNTIME_LIBRARY}")
else()
    set(ENABLE_ML_INFERENCE OFF)
    set(ONNXRUNTIME_LIBRARIES "")
    message(WARNING "⚠️ 未找到 ONNX Runtime，禁用 ML 推理功能")
endif()

# ============================================================================
# Hyperscan/Vectorscan 配置
# ============================================================================

find_library(HYPERSCAN_LIBRARY 
    NAMES hs hyperscan
    PATHS ${THIRD_PARTY_ROOT}/lib /opt/homebrew/lib
)

find_path(HYPERSCAN_INCLUDE_DIR
    NAMES hs/hs.h
    PATHS ${THIRD_PARTY_ROOT}/include /opt/homebrew/include
)

if(HYPERSCAN_LIBRARY AND HYPERSCAN_INCLUDE_DIR)
    add_library(Hyperscan::hyperscan UNKNOWN IMPORTED)
    set_target_properties(Hyperscan::hyperscan PROPERTIES
        IMPORTED_LOCATION "${HYPERSCAN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${HYPERSCAN_INCLUDE_DIR}"
    )
    set(HYPERSCAN_LIBRARIES Hyperscan::hyperscan)
    message(STATUS "✅ 找到 Hyperscan/Vectorscan: ${HYPERSCAN_LIBRARY}")
else()
    message(WARNING "⚠️ Hyperscan/Vectorscan 库未找到，将禁用模式匹配功能")
    set(HYPERSCAN_LIBRARIES "")
endif()

# ============================================================================
# 创建简化的prometheus-cpp替代
# ============================================================================

# 创建简化的metrics库
add_library(simple_metrics INTERFACE)
target_include_directories(simple_metrics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ============================================================================
# 核心库
# ============================================================================

add_subdirectory(src)

# ============================================================================
# GoogleTest 配置
# ============================================================================

find_library(GTEST_LIBRARY 
    NAMES gtest
    PATHS ${THIRD_PARTY_ROOT}/lib
    NO_DEFAULT_PATH
)

find_library(GTEST_MAIN_LIBRARY 
    NAMES gtest_main
    PATHS ${THIRD_PARTY_ROOT}/lib
    NO_DEFAULT_PATH
)

if(GTEST_LIBRARY AND GTEST_MAIN_LIBRARY AND EXISTS "${THIRD_PARTY_ROOT}/include/gtest/gtest.h")
    add_library(GTest::gtest UNKNOWN IMPORTED)
    set_target_properties(GTest::gtest PROPERTIES
        IMPORTED_LOCATION "${GTEST_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/include"
    )
    
    add_library(GTest::gtest_main UNKNOWN IMPORTED)
    set_target_properties(GTest::gtest_main PROPERTIES
        IMPORTED_LOCATION "${GTEST_MAIN_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_ROOT}/include"
    )
    
    set(GTEST_FOUND ON)
    message(STATUS "✅ 找到 GoogleTest: ${GTEST_LIBRARY}")
else()
    set(GTEST_FOUND OFF)
    message(WARNING "⚠️ GoogleTest 未找到，将禁用测试")
endif()

# ============================================================================
# 测试配置
# ============================================================================

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING AND GTEST_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# 主可执行文件
# ============================================================================

add_executable(dmp_server src/main.cpp)

# 链接所有依赖 - 修复版本
target_link_libraries(dmp_server PRIVATE
    dmp_core
    fmt                     # Header-only fmt
    spdlog::spdlog         # spdlog 日志库
    simdjson::simdjson
    phmap
    tomlplusplus
    simple_metrics         # 简化metrics库
    ${HYPERSCAN_LIBRARIES}
    ${ONNXRUNTIME_LIBRARIES}
    Threads::Threads
)

# 应用编译器优化选项
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake)
set_optimization_flags(dmp_server)

# ============================================================================
# 安装配置
# ============================================================================

install(TARGETS dmp_server DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/dmp)

# ============================================================================
# 构建信息显示
# ============================================================================

message(STATUS "")
message(STATUS "🚀 DMP 风控系统构建配置 - 修复版本")
message(STATUS "======================================")
message(STATUS "📁 第三方库路径: ${THIRD_PARTY_ROOT}")
message(STATUS "🏗️ 构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "🖥️ 目标架构: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "🧠 ML 推理: ${ENABLE_ML_INFERENCE}")
message(STATUS "🔍 模式匹配: ${HYPERSCAN_LIBRARIES}")
message(STATUS "🧪 构建测试: ${BUILD_TESTING}")
message(STATUS "")
