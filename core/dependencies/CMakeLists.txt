if(NOT WIN32) # D-Bus is the Linux platform layer
    add_subdirectory(sdbus-cpp)
endif()
add_subdirectory(uSockets)
add_subdirectory(uWebSockets)
add_subdirectory(json)
add_subdirectory(toml++)
