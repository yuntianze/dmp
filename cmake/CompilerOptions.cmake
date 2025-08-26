# CompilerOptions.cmake - 编译器优化配置

function(set_optimization_flags target)
    # CPU 特性检测
    include(CheckCXXCompilerFlag)
    
    # 检查 AVX2 支持
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(${target} PRIVATE -mavx2)
        message(STATUS "启用 AVX2 优化")
    endif()
    
    # 检查 AVX512 支持
    check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
    if(COMPILER_SUPPORTS_AVX512)
        target_compile_options(${target} PRIVATE -mavx512f)
        message(STATUS "启用 AVX512 优化")
    endif()
    
    # 检查 FMA 支持
    check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
    if(COMPILER_SUPPORTS_FMA)
        target_compile_options(${target} PRIVATE -mfma)
        message(STATUS "启用 FMA 优化")
    endif()
    
    # Release 优化选项
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Release>:
            -O3
            -march=native
            -mtune=native
            -flto
            -fomit-frame-pointer
            -funroll-loops
            -fprefetch-loop-arrays
            -ffast-math
            -DNDEBUG
        >
        $<$<CONFIG:Debug>:
            -O0
            -g3
            -fno-omit-frame-pointer
            -fsanitize=address
            -fsanitize=undefined
            -fsanitize=leak
            -Wall
            -Wextra
            -Wpedantic
        >
    )
    
    # 链接时优化
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endfunction()
