enable_testing()

cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_bej_roundtrip test_bej_roundtrip.cpp)
target_link_libraries(test_bej_roundtrip PRIVATE
        json_lib
        bej_lib
        GTest::gtest_main
)

file(COPY ${CMAKE_SOURCE_DIR}/tests/data DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${CMAKE_SOURCE_DIR}/tests/dictionaries DESTINATION ${CMAKE_BINARY_DIR}/tests)

include(GoogleTest)
gtest_discover_tests(test_bej_roundtrip)
