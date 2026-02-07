cmake_minimum_required(VERSION 3.28)
project(WinAgent)

set(CMAKE_CXX_STANDARD 20)

# 1. MSVC İÇİN STATİK BAĞLAMA (KRİTİK)
# Bu satır, C++ kütüphanelerini .exe'nin içine gömer.
# Böylece karşı bilgisayarda Visual C++ kütüphanelerinin kurulu olmasına gerek kalmaz.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 2. MINGW kullanırsan diye senin orijinal bayrakların
if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif ()

include_directories(include)

add_executable(WinAgent main.cpp
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
        include/modules/AudioDeviceMonitor.h
        src/modules/AudioDeviceMonitor.cpp
        include/modules/AudioDeviceSwitcher.h
        src/modules/AudioDeviceSwitcher.cpp)

# Windows sistem kütüphaneleri (Zaten her Windows'ta vardır, sorun çıkarmazlar)
target_link_libraries(WinAgent PRIVATE
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

# Winsock çakışmalarını önlemek için global tanım
add_compile_definitions(WIN32_LEAN_AND_MEAN)