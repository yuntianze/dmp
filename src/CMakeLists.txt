# 核心库构建配置

# 收集所有源文件
file(GLOB_RECURSE DMP_SOURCES
    "server/*.cpp"
    "engine/*.cpp"
    "feature/*.cpp"
    "inference/*.cpp"
    "monitor/*.cpp"
)

# 创建核心库
add_library(dmp_core STATIC ${DMP_SOURCES})

# 包含目录
target_include_directories(dmp_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 链接依赖
target_link_libraries(dmp_core PUBLIC
    Drogon::Drogon
    spdlog::spdlog
    fmt::fmt
    simdjson::simdjson
    phmap
    tomlplusplus::tomlplusplus
    prometheus-cpp::core
    Threads::Threads
)

# 应用编译器优化
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CompilerOptions.cmake)
set_optimization_flags(dmp_core)
