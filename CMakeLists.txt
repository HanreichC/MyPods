cmake_minimum_required(VERSION 3.22.1)

project(mypods VERSION 0.2.0 LANGUAGES C CXX)

# core/ = Daemon (AAP ueber L2CAP + BLE-Advertisements, WebSocket-API auf :2020)
# ui/   = Qt6/QML-Oberflaeche, startet den Daemon und spricht ihn ueber WebSocket an
add_subdirectory(core)
add_subdirectory(ui)

# Die UI sucht den Daemon unter <verzeichnis der binary>/modules/magicpodscore
set_target_properties(magicpods PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(magicpodscore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
