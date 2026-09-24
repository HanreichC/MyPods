cmake_minimum_required(VERSION 3.22.1)

project(mypods VERSION 0.2.0 LANGUAGES C CXX)

# The version the app and the daemon show: the project version plus the commit, in the same form as the
# Arch package's pkgver (0.2.0.r3.gabc1234: 3 commits after tag v0.2.0), so the About page matches what
# pacman, the nightly or a self-built binary say. ".dirty" marks a build with uncommitted changes.
set(MYPODS_VERSION ${PROJECT_VERSION})
find_package(Git QUIET)
if(GIT_FOUND)
    # safe.directory: the source may belong to another user, e.g. mounted into a build container
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* describe --long --tags --match "v*" --dirty
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_DESCRIBE
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(GIT_DESCRIBE MATCHES "^v[0-9.]+-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
        set(MYPODS_VERSION "${PROJECT_VERSION}.r${CMAKE_MATCH_1}.g${CMAKE_MATCH_2}")
    else() # no tag yet: count every commit, like the PKGBUILD
        execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* rev-list --count HEAD
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* describe --always --dirty
                        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(GIT_COUNT MATCHES "^[0-9]+$" AND GIT_DESCRIBE MATCHES "^([0-9a-f]+)(-dirty)?$")
            set(MYPODS_VERSION "${PROJECT_VERSION}.r${GIT_COUNT}.g${CMAKE_MATCH_1}")
        endif()
    endif()
    if(GIT_DESCRIBE MATCHES "-dirty$")
        string(APPEND MYPODS_VERSION ".dirty")
    endif()
endif()
message(STATUS "MyPods version ${MYPODS_VERSION}")

# core/ = Daemon (AAP ueber L2CAP + BLE-Advertisements, WebSocket-API auf :2020)
# ui/   = Qt6/QML-Oberflaeche, startet den Daemon und spricht ihn ueber WebSocket an
add_subdirectory(core)
add_subdirectory(ui)

# Die UI sucht den Daemon unter <verzeichnis der binary>/modules/magicpodscore
set_target_properties(magicpods PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(magicpodscore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
