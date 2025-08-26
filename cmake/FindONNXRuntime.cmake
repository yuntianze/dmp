# FindONNXRuntime.cmake - 查找 ONNX Runtime 库

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS
        /usr/include/onnxruntime
        /usr/local/include/onnxruntime
        /opt/homebrew/include/onnxruntime
        ${ONNXRUNTIME_ROOT}/include
    PATH_SUFFIXES core/session
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
        ${ONNXRUNTIME_ROOT}/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
    REQUIRED_VARS ONNXRUNTIME_LIBRARY ONNXRUNTIME_INCLUDE_DIR
)

if(ONNXRuntime_FOUND)
    set(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIBRARY})
    set(ONNXRUNTIME_INCLUDE_DIRS ${ONNXRUNTIME_INCLUDE_DIR})
    
    if(NOT TARGET ONNXRuntime::onnxruntime)
        add_library(ONNXRuntime::onnxruntime UNKNOWN IMPORTED)
        set_target_properties(ONNXRuntime::onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY)
