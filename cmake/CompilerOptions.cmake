# CompilerOptions.cmake - 编译器优化配置

function(set_optimization_flags target)
    # CPU 特性检测
    include(CheckCXXCompilerFlag)
    
    # Apple Silicon (ARM64) 优化
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        # Apple Silicon 特定优化
        check_cxx_compiler_flag("-mcpu=apple-m1" COMPILER_SUPPORTS_APPLE_M1)
        if(COMPILER_SUPPORTS_APPLE_M1)
            target_compile_options(${target} PRIVATE -mcpu=apple-m1)
            message(STATUS "启用 Apple M1/M2/M3 优化")
        endif()
        
        # ARM NEON 支持 (Apple Silicon 不需要 -mfpu=neon)
        # Apple Silicon 默认已启用 NEON
        message(STATUS "启用 ARM NEON 优化")
    else()
        # x86_64 优化
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2)
            target_compile_options(${target} PRIVATE -mavx2)
            message(STATUS "启用 AVX2 优化")
        endif()
        
        check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
        if(COMPILER_SUPPORTS_AVX512)
            target_compile_options(${target} PRIVATE -mavx512f)
            message(STATUS "启用 AVX512 优化")
        endif()
    endif()
    
    # 通用 FMA 支持检查
    check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
    if(COMPILER_SUPPORTS_FMA)
        target_compile_options(${target} PRIVATE -mfma)
        message(STATUS "启用 FMA 优化")
    endif()
    
    # Release 优化选项
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Release>:
            -O3
            -flto
            -fomit-frame-pointer
            -funroll-loops
            -ffast-math
            -DNDEBUG
        >
        $<$<CONFIG:Debug>:
            -O0
            -g3
            -fno-omit-frame-pointer
            -fsanitize=address
            -fsanitize=undefined
            -Wall
            -Wextra
            -Wpedantic
        >
    )
    
    # 链接时优化
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endfunction()
