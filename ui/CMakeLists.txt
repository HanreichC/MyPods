cmake_minimum_required(VERSION 3.16)

project(magicpods VERSION 1.0.2 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 6.9: RectangularShadow
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets WebSockets Linguist)
if(NOT WIN32)
    # MPRIS for the tray popup's now playing (Windows asks the system media sessions instead)
    find_package(Qt6 6.9 REQUIRED COMPONENTS DBus)
    # Optional: anchors the tray popup under the panel on Wayland (KDE ships it; the AppImage builds without)
    find_package(LayerShellQt QUIET)
endif()

qt_standard_project_setup(REQUIRES 6.8
    I18N_TRANSLATED_LANGUAGES en de
)

set(QML_IMPORT_PATH "${CMAKE_BINARY_DIR}/qml" CACHE STRING "Qt Creator extra qml import paths" FORCE)

set(APP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/app")
set(APP_QML_DIR "${APP_SOURCE_DIR}/qml")
set(APP_I18N_TS_FILE_BASE "src/app/i18n/locale")

set_source_files_properties(src/app/qml/Units.qml src/app/qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

file(GLOB_RECURSE APP_RESOURCE_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_QML_DIR}/assets/*"
)

qt_add_executable(magicpods
    src/app/cpp/Backend.cpp
    src/app/cpp/BackendManager.cpp
    src/app/cpp/DesktopManager.cpp
    src/app/cpp/main.cpp
    # listed for AUTOMOC: its implementation is MediaController.cpp or MediaController_win.cpp per platform
    src/app/cpp/MediaController.h
    src/app/cpp/TrayIcon.cpp
    src/app/cpp/TrayIconManager.cpp
)

qt_add_resources(magicpods "fonts"
    PREFIX "/fonts"
    FILES
        src/app/fonts/Inter-Regular.ttf
        src/app/fonts/Inter-Medium.ttf
        src/app/fonts/Inter-SemiBold.ttf
        src/app/fonts/Inter-Bold.ttf
)

qt_add_qml_module(magicpods
    URI magicpods
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/magicpods"
    QML_FILES
        src/app/qml/Units.qml
        src/app/qml/Theme.qml
        src/app/qml/Main.qml
        src/app/qml/PopupAnimation.qml
        src/app/qml/TrayPopup.qml
        src/app/qml/components/Animations.qml
        src/app/qml/components/BatteryRing.qml
        src/app/qml/components/FormRow.qml
        src/app/qml/components/Card.qml
        src/app/qml/components/DeviceImage.qml
        src/app/qml/components/Heading.qml
        src/app/qml/components/HelpMessage.qml
        src/app/qml/components/InlineMessage.qml
        src/app/qml/components/Label.qml
        src/app/qml/components/NoiseControl.qml
        src/app/qml/components/Picker.qml
        src/app/qml/components/ScrollPage.qml
        src/app/qml/components/Toggle.qml
        src/app/qml/components/Separator.qml
        src/app/qml/pages/AboutPage.qml
        src/app/qml/pages/BatteryPage.qml
        src/app/qml/pages/HeadphonesPage.qml
        src/app/qml/pages/SettingsPage.qml
        src/app/qml/pages/ColorSample.qml
    RESOURCES
        ${APP_RESOURCE_FILES}
)

set_target_properties(magicpods PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(magicpods PRIVATE
    src/app/cpp
    src/app/dependencies
)

target_link_libraries(magicpods PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::WebSockets
)

if(WIN32)
    # magicpods.rc: the exe's icon (Explorer, taskbar, installer shortcuts)
    target_sources(magicpods PRIVATE src/app/cpp/MediaController_win.cpp src/app/magicpods.rc)
    set_target_properties(magicpods PROPERTIES CXX_STANDARD 20) # C++/WinRT's coroutines
    target_compile_definitions(magicpods PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(magicpods PRIVATE /utf-8 /bigobj)
    target_link_libraries(magicpods PRIVATE windowsapp ole32)
else()
    target_sources(magicpods PRIVATE src/app/cpp/MediaController.cpp)
    target_link_libraries(magicpods PRIVATE Qt6::DBus)
endif()

if(LayerShellQt_FOUND)
    target_link_libraries(magicpods PRIVATE LayerShellQt::Interface)
    target_compile_definitions(magicpods PRIVATE HAVE_LAYERSHELLQT)
endif()

target_compile_definitions(magicpods PRIVATE
    MAGICPODS_VERSION="${PROJECT_VERSION}"
    MAGICPODS_APP_SOURCE_DIR="${APP_SOURCE_DIR}"
)

qt_add_translations(magicpods
    SOURCE_TARGETS magicpods
    TS_FILE_BASE "${APP_I18N_TS_FILE_BASE}"
    # the UI uses qsTrId; without it lrelease (e.g. Qt 6.9) drops every message
    LRELEASE_OPTIONS -idbased
    MERGE_QT_TRANSLATIONS
    QT_TRANSLATION_CATALOGS qtbase
    RESOURCE_PREFIX i18n
)

install(TARGETS magicpods
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET magicpods
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})
