cmake_minimum_required(VERSION 3.28)
project(WinAgent)

set(CMAKE_CXX_STANDARD 20)

# Qt build helpers:
# - AUTOMOC: runs Qt's Meta-Object Compiler (needed for signals/slots, Q_OBJECT, etc.)
# - AUTOUIC: auto-generates code from .ui files (Qt Designer)
# - AUTORCC: auto-generates code from .qrc resource files
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt modules used by this app:
# - Widgets: UI (QMainWindow, QLabel, QPushButton, ...)
# - Network: networking helpers (some parts still use WinSock directly)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network HttpServer WebSockets)

include_directories(include)

# ---- Runtime assets / external files ----
set(WA_CERTS_DIR "" CACHE PATH "Path to certificate folder to copy next to the app (outside build dir recommended)")

function(wa_copy_files_post_build target dest_dir)
    foreach(f IN LISTS ARGN)
        if (EXISTS "${f}")
            add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${dest_dir}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}" "${dest_dir}"
            )
        else()
            message(WARNING "wa_copy_files_post_build: missing file: ${f}")
        endif()
    endforeach()
endfunction()

function(wa_copy_dir_post_build target src_dir dest_dir)
    if (EXISTS "${src_dir}")
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${dest_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${src_dir}" "${dest_dir}"
        )
    else()
        message(WARNING "wa_copy_dir_post_build: missing dir: ${src_dir}")
    endif()
endfunction()

# Application sources and headers.
add_executable(WinAgent WIN32
        main.cpp

        src/MainWindow.cpp
        src/DashboardServer.cpp
        src/DashboardWebSocketServer.cpp
        src/BasePlugin.cpp
        src/PluginManager.cpp

        include/MainWindow.h
        include/DashboardServer.h
        include/DashboardWebSocketServer.h
        include/BasePlugin.h
        include/PluginManager.h

        include/Logger.h
        icon.rc
)

# -------------------------
# Plugins (external DLLs)
# -------------------------
add_subdirectory(plugins)

# Make sure libraries under project_root/lib can be found by name (e.g. hidapi)
target_link_directories(WinAgent PRIVATE "${CMAKE_SOURCE_DIR}/lib")

# Link against Qt and Windows system libraries used by different monitors.
target_link_libraries(WinAgent PRIVATE Qt6::Widgets Qt6::Network Qt6::HttpServer Qt6::WebSockets)

# -------------------------
# Post-build deployment (copy runtime deps/assets + Qt deploy)
# -------------------------
set(WA_RUNTIME_DIR "$<TARGET_FILE_DIR:WinAgent>")

# Copy dashboard/default -> next to exe (dashboard/default)
wa_copy_dir_post_build(WinAgent "${CMAKE_SOURCE_DIR}/dashboards/default" "${WA_RUNTIME_DIR}/dashboards/default")

# Copy certificates (recommended: keep outside build dir)
# Example configure: -DWA_CERTS_DIR="C:/Users/tugru/WinAgentCerts"
if (WA_CERTS_DIR)
    wa_copy_dir_post_build(WinAgent "${WA_CERTS_DIR}" "${WA_RUNTIME_DIR}/certs")
else()
    wa_copy_dir_post_build(WinAgent "${CMAKE_SOURCE_DIR}/certs" "${WA_RUNTIME_DIR}/certs")
    message(STATUS "WA_CERTS_DIR not set; using ./certs .")
endif()

# Run windeployqt automatically after build (DEV deploy)
if (WIN32)
    # Build tool'a verdiğin CMAKE_PREFIX_PATH genelde Qt root'u içerir -> bin/windeployqt.exe
    set(_qt_hint_bins "")
    foreach(p IN LISTS CMAKE_PREFIX_PATH)
        list(APPEND _qt_hint_bins "${p}/bin")
    endforeach()

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${_qt_hint_bins})

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET WinAgent POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                $<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>
                --no-translations
                --dir "${WA_RUNTIME_DIR}"
                "$<TARGET_FILE:WinAgent>"
                COMMENT "Running windeployqt"
        )
    else()
        message(WARNING "windeployqt not found; Qt deployment skipped.")
    endif()
endif()

# Deployment logic
install(TARGETS WinAgent RUNTIME DESTINATION bin)

# Copy icon file for system tray
install(FILES "${CMAKE_SOURCE_DIR}/app_icon.ico" DESTINATION bin)

# Dashboards -> <prefix>/bin/dashboards/default
install(FILES "${CMAKE_SOURCE_DIR}/dashboards/default/index.html" DESTINATION bin/dashboards/default)
install(FILES "${CMAKE_SOURCE_DIR}/dashboards/default/default.css" DESTINATION bin/dashboards/default)

# Certs -> <prefix>/bin/certs
if (WA_CERTS_DIR)
    install(DIRECTORY "${WA_CERTS_DIR}" DESTINATION bin)
else()
    install(FILES "${CMAKE_SOURCE_DIR}/certs/cert.pem" DESTINATION bin/certs)
    install(FILES "${CMAKE_SOURCE_DIR}/certs/key.pem" DESTINATION bin/certs)
endif()

target_link_options(WinAgent PRIVATE "/SUBSYSTEM:WINDOWS")

add_compile_definitions(WIN32_LEAN_AND_MEAN)
