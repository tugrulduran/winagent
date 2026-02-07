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
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

include_directories(include)

# Application sources and headers.
# Note: headers are listed so IDEs show them nicely; CMake does not compile headers by themselves.
add_executable(WinAgent
        main.cpp
        include/BaseMonitor.h
        include/modules/CPUMonitor.h
        src/modules/CPUMonitor.cpp
        include/modules/MemoryMonitor.h
        src/modules/MemoryMonitor.cpp
        include/ModuleFactory.h
        include/NetworkReporter.h
        include/modules/NetworkBytes.h
        src/modules/NetworkBytes.cpp
        src/NetworkReporter.cpp
        include/modules/AudioMonitor.h
        src/modules/AudioMonitor.cpp
        include/modules/MediaMonitor.h
        src/modules/MediaMonitor.cpp
        include/modules/AudioDeviceSwitcher.h
        src/modules/AudioDeviceSwitcher.cpp
        include/MainWindow.h
        src/MainWindow.cpp
        include/modules/ProcessMonitor.h
)

# Link against Qt and Windows system libraries used by different monitors.
target_link_libraries(WinAgent PRIVATE
        Qt6::Widgets
        Qt6::Network
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
)

# If you want a pure GUI app without a console window, enable this:
# set_target_properties(WinAgent PROPERTIES WIN32_EXECUTABLE TRUE)

add_compile_definitions(WIN32_LEAN_AND_MEAN)