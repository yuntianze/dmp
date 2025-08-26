# FindHyperscan.cmake - 查找 Hyperscan 库

find_path(HYPERSCAN_INCLUDE_DIR
    NAMES hs/hs.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/homebrew/include
)

find_library(HYPERSCAN_LIBRARY
    NAMES hs hyperscan
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/homebrew/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hyperscan
    REQUIRED_VARS HYPERSCAN_LIBRARY HYPERSCAN_INCLUDE_DIR
)

if(Hyperscan_FOUND)
    set(HYPERSCAN_LIBRARIES ${HYPERSCAN_LIBRARY})
    set(HYPERSCAN_INCLUDE_DIRS ${HYPERSCAN_INCLUDE_DIR})
    
    if(NOT TARGET Hyperscan::hyperscan)
        add_library(Hyperscan::hyperscan UNKNOWN IMPORTED)
        set_target_properties(Hyperscan::hyperscan PROPERTIES
            IMPORTED_LOCATION "${HYPERSCAN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HYPERSCAN_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(HYPERSCAN_INCLUDE_DIR HYPERSCAN_LIBRARY)
