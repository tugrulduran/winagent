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
find_package(Qt6 REQUIRED COMPONENTS Widgets Network HttpServer WebSockets)

include_directories(include)

# Application sources and headers.
# Note: headers are listed so IDEs show them nicely; CMake does not compile headers by themselves.
add_executable(WinAgent WIN32
        main.cpp
        include/MainWindow.h
        include/ModuleFactory.h
        include/BaseMonitor.h
        include/modules/CPUMonitor.h
        include/modules/MemoryMonitor.h
        include/modules/NetworkMonitor.h
        include/modules/ProcessMonitor.h
        include/modules/AudioMonitor.h
        include/modules/MediaMonitor.h
        include/modules/AudioDeviceMonitor.h
        include/modules/AudezeMonitor.h
        include/modules/LauncherMonitor.h
        include/Dashboard.h
        include/DashboardData.h
        include/DashboardServer.h
        include/DashboardWebSocketServer.h

        src/MainWindow.cpp
        src/modules/CPUMonitor.cpp
        src/modules/MemoryMonitor.cpp
        src/modules/NetworkMonitor.cpp
        src/modules/ProcessMonitor.cpp
        src/modules/AudioMonitor.cpp
        src/modules/MediaMonitor.cpp
        src/modules/AudioDeviceMonitor.cpp
        src/modules/AudezeMonitor.cpp
        src/DashboardServer.cpp
        src/DashboardWebSocketServer.cpp
        src/modules/LauncherMonitor.cpp

        include/Logger.h
)

# Link against Qt and Windows system libraries used by different monitors.
target_link_libraries(WinAgent PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::HttpServer
        Qt6::WebSockets
        psapi
        pdh
        ws2_32
        iphlpapi
        runtimeobject
        ole32
        oleaut32
        shlwapi
        version
        user32
        advapi32
        uuid
        hidapi
        setupapi
)

# Deployment logic
install(TARGETS WinAgent
        RUNTIME DESTINATION bin
)

target_link_options(WinAgent PRIVATE
        "/SUBSYSTEM:WINDOWS"
)

add_compile_definitions(WIN32_LEAN_AND_MEAN)