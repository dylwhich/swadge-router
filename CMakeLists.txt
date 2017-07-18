project(swadge_router CXX)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/packets.cc
        src/packets.h
        src/server.cc
        src/server.h
        src/main.cc)

add_executable(swadge_router ${SOURCE_FILES})
