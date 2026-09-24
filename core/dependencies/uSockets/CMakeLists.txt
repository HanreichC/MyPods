# uNetworking/uSockets v0.8.7 (Apache-2.0), vendored src/
cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_C_STANDARD 11)
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/vendor/src/*.c)
add_library(uSockets ${SOURCES})
target_include_directories(uSockets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/src)
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL)
if(WIN32)
    # libusockets.h picks libuv on Windows (epoll/kqueue elsewhere); from vcpkg there
    find_package(libuv CONFIG REQUIRED)
    target_link_libraries(uSockets PUBLIC $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv> ws2_32)
endif()
