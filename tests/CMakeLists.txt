enable_testing()

cmake_policy(SET CMP0135 NEW)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(encode_compare_tests test_encode_compare.cpp)
target_link_libraries(encode_compare_tests PRIVATE
        json_lib
        bej_lib
        GTest::gtest_main
)

file(COPY ${CMAKE_SOURCE_DIR}/tests/data DESTINATION ${CMAKE_BINARY_DIR}/tests)
file(COPY ${CMAKE_SOURCE_DIR}/tests/dictionaries DESTINATION ${CMAKE_BINARY_DIR}/tests)

include(GoogleTest)
gtest_discover_tests(encode_compare_tests)
#enable_testing()
#
#cmake_policy(SET CMP0135 NEW)
#
#include(FetchContent)
#FetchContent_Declare(
#        googletest
#        URL https://github.com/google/googletest/archive/refs/heads/main.zip
#)
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(googletest)
#
#add_executable(json_tests test_json.cpp)
#add_executable(bej_tests test_bej.cpp)
#target_link_libraries(json_tests PRIVATE
#        json_lib
#        bej_lib
#        GTest::gtest_main
#)
#
#include(GoogleTest)
#gtest_discover_tests(json_tests)
