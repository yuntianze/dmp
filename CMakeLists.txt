cmake_minimum_required(VERSION 3.20)
project(dmp_prototype VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG -flto -fomit-frame-pointer -funroll-loops")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fno-omit-frame-pointer -fsanitize=address,undefined,leak")

# LTO 优化
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# 模块路径
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 启用 ExternalProject 和 FetchContent
include(ExternalProject)
include(FetchContent)

# ============================================================================
# 依赖管理（按设计文档）
# ============================================================================

# 基础依赖
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# Drogon Web 框架
find_package(Drogon CONFIG REQUIRED)

# spdlog 日志库
find_package(spdlog CONFIG REQUIRED)

# fmt 格式化库
find_package(fmt CONFIG REQUIRED)

# TOML++ 配置解析
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# simdjson 高性能 JSON 解析
FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(simdjson)

# parallel-hashmap 高性能哈希表
FetchContent_Declare(
    parallel_hashmap
    GIT_REPOSITORY https://github.com/greg7mdp/parallel-hashmap.git
    GIT_TAG        v1.3.11
)
FetchContent_MakeAvailable(parallel_hashmap)

# BS::thread_pool 现代线程池
FetchContent_Declare(
    thread_pool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
    GIT_TAG        v4.1.0
)
FetchContent_MakeAvailable(thread_pool)

# ExprTk 表达式引擎（header-only）
FetchContent_Declare(
    exprtk
    GIT_REPOSITORY https://github.com/ArashPartow/exprtk.git
    GIT_TAG        0.0.2
)
FetchContent_MakeAvailable(exprtk)

# prometheus-cpp 监控指标
find_package(prometheus-cpp CONFIG REQUIRED)

# 自定义查找模块
find_package(Hyperscan REQUIRED)

# ONNX Runtime（可选，用于 ML 推理）
find_package(ONNXRuntime QUIET)
if(ONNXRuntime_FOUND)
    message(STATUS "找到 ONNX Runtime，启用 ML 推理功能")
    set(ENABLE_ML_INFERENCE ON)
else()
    message(WARNING "未找到 ONNX Runtime，禁用 ML 推理功能")
    set(ENABLE_ML_INFERENCE OFF)
    set(ONNXRUNTIME_LIBRARIES "")
endif()

# ============================================================================
# 核心库
# ============================================================================

add_subdirectory(src)

# ============================================================================
# 主可执行文件
# ============================================================================

add_executable(dmp_server src/main.cpp)

# 链接所有依赖
target_link_libraries(dmp_server PRIVATE
    dmp_core
    Drogon::Drogon
    spdlog::spdlog
    fmt::fmt
    simdjson::simdjson
    phmap
    tomlplusplus::tomlplusplus
    prometheus-cpp::core
    prometheus-cpp::pull
    ${HYPERSCAN_LIBRARIES}
    ${ONNXRUNTIME_LIBRARIES}
    Threads::Threads
)

# 编译器特定优化
target_compile_options(dmp_server PRIVATE
    $<$<CONFIG:Release>:
        -mavx2
        -mfma
        -fprefetch-loop-arrays
        -ffast-math
    >
)

# 安装配置
install(TARGETS dmp_server DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/dmp)
install(DIRECTORY models/ DESTINATION share/dmp/models)
install(DIRECTORY data/ DESTINATION share/dmp/data)
