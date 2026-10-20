cmake_minimum_required(VERSION 3.16)

project(mypods VERSION 1.0.2 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 6.9: RectangularShadow
find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets WebSockets Network Linguist ShaderTools)
if(NOT WIN32)
    # MPRIS for the tray popup's now playing (Windows asks the system media sessions instead)
    find_package(Qt6 6.9 REQUIRED COMPONENTS DBus)
    # Optional: anchors the tray popup under the panel on Wayland (KDE ships it; the AppImage builds without)
    find_package(LayerShellQt QUIET)
    # Optional: KWin blurs the desktop behind the tray popup and the lid popup (Liquid Glass on KDE)
    find_package(KF6WindowSystem QUIET)
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

qt_add_executable(mypods
    src/app/cpp/Backend.cpp
    src/app/cpp/BackendManager.cpp
    src/app/cpp/DesktopManager.cpp
    src/app/cpp/main.cpp
    # listed for AUTOMOC: its implementation is MediaController.cpp or MediaController_win.cpp per platform
    src/app/cpp/MediaController.h
    src/app/cpp/MediaController_cover.cpp
    src/app/cpp/TrayIcon.cpp
    src/app/cpp/TrayIconManager.cpp
)

qt_add_resources(mypods "fonts"
    PREFIX "/fonts"
    BASE src/app/fonts # :/fonts/Inter-Regular.ttf, as main.cpp loads them
    FILES
        src/app/fonts/Inter-Regular.ttf
        src/app/fonts/Inter-Medium.ttf
        src/app/fonts/Inter-SemiBold.ttf
        src/app/fonts/Inter-Bold.ttf
)

qt_add_qml_module(mypods
    URI mypods
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/mypods"
    QML_FILES
        src/app/qml/Units.qml
        src/app/qml/Theme.qml
        src/app/qml/Main.qml
        src/app/qml/PopupAnimation.qml
        src/app/qml/TrayPopup.qml
        src/app/qml/components/Animations.qml
        src/app/qml/components/BatteryRing.qml
        src/app/qml/components/FormRow.qml
        src/app/qml/components/InfoButton.qml
        src/app/qml/components/Card.qml
        src/app/qml/components/DeviceImage.qml
        src/app/qml/components/Heading.qml
        src/app/qml/components/HelpMessage.qml
        src/app/qml/components/InlineMessage.qml
        src/app/qml/components/Label.qml
        src/app/qml/components/NoiseControl.qml
        src/app/qml/components/Picker.qml
        src/app/qml/components/ResponseCurve.qml
        src/app/qml/components/ScrollPage.qml
        src/app/qml/components/Toggle.qml
        src/app/qml/components/Separator.qml
        src/app/qml/components/Glass.qml
        src/app/qml/components/Button.qml
        src/app/qml/components/Slider.qml
        src/app/qml/components/TextField.qml
        src/app/qml/components/CheckBox.qml
        src/app/qml/components/ScrollBar.qml
        src/app/qml/components/NowPlaying.qml
        src/app/qml/pages/AboutPage.qml
        src/app/qml/pages/DevicePage.qml
        src/app/qml/pages/BluetoothPage.qml
        src/app/qml/pages/SettingsPage.qml
    RESOURCES
        ${APP_RESOURCE_FILES}
)

# MyCAD's glass (Glass.qml); qsb compiles it for every graphics API, at build time only
qt_add_shaders(mypods "shaders"
    PREFIX "/qt/qml/mypods"
    FILES
        src/app/shaders/glass.frag
)

set_target_properties(mypods PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(mypods PRIVATE
    src/app/cpp
    src/app/dependencies
)

target_link_libraries(mypods PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Network
)

if(WIN32)
    # mypods.rc: the exe's icon (Explorer, taskbar, installer shortcuts)
    target_sources(mypods PRIVATE src/app/cpp/MediaController_win.cpp src/app/mypods.rc)
    set_target_properties(mypods PROPERTIES CXX_STANDARD 20) # C++/WinRT's coroutines
    target_compile_definitions(mypods PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(mypods PRIVATE /utf-8 /bigobj)
    target_link_libraries(mypods PRIVATE windowsapp ole32)
else()
    target_sources(mypods PRIVATE src/app/cpp/MediaController.cpp)
    target_link_libraries(mypods PRIVATE Qt6::DBus)
endif()

if(LayerShellQt_FOUND)
    target_link_libraries(mypods PRIVATE LayerShellQt::Interface)
    target_compile_definitions(mypods PRIVATE HAVE_LAYERSHELLQT)
endif()

if(KF6WindowSystem_FOUND)
    target_link_libraries(mypods PRIVATE KF6::WindowSystem)
    target_compile_definitions(mypods PRIVATE HAVE_KWINDOWSYSTEM)
endif()

target_compile_definitions(mypods PRIVATE
    MYPODS_VERSION="${MYPODS_VERSION}"
    MYPODS_APP_SOURCE_DIR="${APP_SOURCE_DIR}"
)

qt_add_translations(mypods
    SOURCE_TARGETS mypods
    TS_FILE_BASE "${APP_I18N_TS_FILE_BASE}"
    # the UI uses qsTrId; without it lrelease (e.g. Qt 6.9) drops every message
    LRELEASE_OPTIONS -idbased
    MERGE_QT_TRANSLATIONS
    QT_TRANSLATION_CATALOGS qtbase
    RESOURCE_PREFIX i18n
)

install(TARGETS mypods
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET mypods
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})
