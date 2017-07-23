cmake_minimum_required(VERSION 3.4)

project(swadge_router CXX)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/wamp.cc
        src/wamp.h
        src/packets.cc
        src/packets.h
        src/server.cc
        src/server.h
        src/main.cc)

add_executable(swadge_router ${SOURCE_FILES})

find_library(LIBWAMPCC libwampcc.a)
find_library(LIBWAMPCC_JSON libwampcc_json.a)
target_link_libraries(swadge_router ${LIBWAMPCC} ${LIBWAMPCC_JSON} pthread ssl crypto jansson uv)
