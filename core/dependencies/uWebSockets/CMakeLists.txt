# uNetworking/uWebSockets v20.58.0 (Apache-2.0), header-only, vendored src/
cmake_minimum_required(VERSION 3.22.1)
find_package(ZLIB REQUIRED)
add_library(uWebSockets INTERFACE)
target_include_directories(uWebSockets INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/src)
target_link_libraries(uWebSockets INTERFACE uSockets ${ZLIB_LIBRARIES})
