cmake_minimum_required(VERSION 3.4)

project(swadge_router CXX)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/wamp.h
        src/packets.cc
        src/packets.h
        src/server.cc
        src/server.h
        src/main.cc)

add_executable(swadge_router ${SOURCE_FILES})

target_link_libraries(swadge_router wampcc wampcc_json pthread)
