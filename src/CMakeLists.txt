set(JSON_SOURCES json.c)
set(BEJ_SOURCES bej_dictionary.c bej_encode.c)

add_library(json_lib STATIC ${JSON_SOURCES})
add_library(bej_lib STATIC ${BEJ_SOURCES}
        ../include/bej_decode.h
        bej_decode.c
        cli_args.c
        ../include/cli_args.h
        cli_encode.c
        cli_decode.c)
target_link_libraries(bej_lib PUBLIC json_lib)

target_include_directories(json_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(bej_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(bej_parser main.c ${JSON_SOURCES} ${BEJ_SOURCES})
target_link_libraries(bej_parser PRIVATE json_lib bej_lib)
