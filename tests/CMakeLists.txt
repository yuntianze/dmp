# Tests CMakeLists.txt

# Enable testing
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Create test executables

# Transaction tests
add_executable(test_transaction unit/test_transaction.cpp)
target_link_libraries(test_transaction
    PRIVATE
    dmp_core
    GTest::gtest_main
    GTest::gtest
    Threads::Threads
)

# Config tests
add_executable(test_config unit/test_config.cpp)
target_link_libraries(test_config
    PRIVATE
    dmp_core
    GTest::gtest_main
    GTest::gtest
    Threads::Threads
)

# Handler tests
add_executable(test_handlers unit/test_handlers.cpp)
target_link_libraries(test_handlers
    PRIVATE
    dmp_core
    GTest::gtest_main
    GTest::gtest
    Threads::Threads
)

# Metrics tests
add_executable(test_metrics unit/test_metrics.cpp)
target_link_libraries(test_metrics
    PRIVATE
    dmp_core
    GTest::gtest_main
    GTest::gtest
    Threads::Threads
)

# Register tests
add_test(NAME TransactionTest COMMAND test_transaction)
add_test(NAME ConfigTest COMMAND test_config)
add_test(NAME HandlerTest COMMAND test_handlers)
add_test(NAME MetricsTest COMMAND test_metrics)

# Set test properties
set_tests_properties(TransactionTest ConfigTest HandlerTest MetricsTest
    PROPERTIES
    ENVIRONMENT "GTEST_OUTPUT=xml:./test_results/"
)
