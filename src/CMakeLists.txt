# 核心库构建配置

# 收集所有源文件
file(GLOB_RECURSE DMP_SOURCES
    "common/*.cpp"
    "core/*.cpp"
    "server/*.cpp"
    "engine/*.cpp"
    "feature/*.cpp"
    "inference/*.cpp"
    "monitor/*.cpp"
    "utils/*.cpp"
)

# 创建核心库
add_library(dmp_core STATIC ${DMP_SOURCES})

# 包含目录 - 确保所有头文件都能被找到
target_include_directories(dmp_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/install/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/src/include
)

# 链接所有必要依赖
target_link_libraries(dmp_core PUBLIC
    fmt                     # Header-only fmt
    spdlog::spdlog         # spdlog 日志库
    simdjson::simdjson
    phmap                  # parallel-hashmap
    tomlplusplus          # toml++ 库
    exprtk                # ExprTk 表达式引擎
    simple_metrics        # 简化metrics库
    ${HYPERSCAN_LIBRARIES}
    ${ONNXRUNTIME_LIBRARIES}
    Threads::Threads
)

# 应用编译器优化
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerOptions.cmake)
set_optimization_flags(dmp_core)
