cmake_minimum_required(VERSION 3.22.1)

project(mypods VERSION 1.0 LANGUAGES C CXX)

# One version everywhere (About page, MSI, Arch package, GitHub release): MAJOR.MINOR from project() plus
# the commits since the one that last changed this project() line, e.g. 1.0.5. The commit that bumps
# MAJOR.MINOR is X.Y.0. Purely numeric, so the MSI and pacman accept it, and it only grows on main.
# Same formula in packaging/arch/PKGBUILD, .github/release.sh and .github/workflows/windows.yml.
# ".dirty" marks a local build with uncommitted changes.
set(MYPODS_VERSION ${PROJECT_VERSION}.0)
find_package(Git QUIET)
if(GIT_FOUND)
    # safe.directory: the source may belong to another user, e.g. mounted into a build container
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* log -1 --format=%H "-G^project.mypods VERSION" -- CMakeLists.txt
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_BASE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* rev-list --count ${GIT_BASE}..HEAD
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(GIT_COUNT MATCHES "^[0-9]+$")
        set(MYPODS_VERSION "${PROJECT_VERSION}.${GIT_COUNT}")
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} -c safe.directory=* describe --always --dirty
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_DESCRIBE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(GIT_DESCRIBE MATCHES "-dirty$")
        string(APPEND MYPODS_VERSION ".dirty")
    endif()
endif()
message(STATUS "MyPods version ${MYPODS_VERSION}")

# core/ = the daemon (AAP over L2CAP, BLE advertisements, WebSocket API on :2020)
# ui/   = the Qt 6 / QML UI, which starts the daemon and talks to it over the WebSocket
add_subdirectory(core)
add_subdirectory(ui)

# The UI looks for the daemon in modules/mypodscore next to its own binary
set_target_properties(mypods PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(mypodscore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
