cmake_minimum_required(VERSION 3.16) # лучше чуть свежее, чем 4.0.2 – такого не существует
project(bej_parser VERSION 1.0.0 LANGUAGES C CXX)

# Разрешаем только GCC и Clang
if (MSVC)
    message(FATAL_ERROR "MSVC compiler is not supported. Please use GCC or Clang.")
endif()

if (NOT (CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_C_COMPILER_ID}. Use GCC or Clang.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    message(STATUS "Compiler: GCC ${CMAKE_C_COMPILER_VERSION}")
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(STATUS "Compiler: Clang ${CMAKE_C_COMPILER_VERSION}")
endif()

add_subdirectory(src)
add_subdirectory(tests)

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    if(EXISTS ${DOXYGEN_IN})
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        add_custom_target(docs
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM
        )
    else()
        message(STATUS "Doxyfile.in not found, docs target not available")
    endif()
else()
    message(STATUS "Doxygen not found, docs target not available")
endif()

install(TARGETS bej_parser DESTINATION bin)
install(FILES include/json.h include/bej.h DESTINATION include)
install(TARGETS json_lib bej_lib DESTINATION lib)
