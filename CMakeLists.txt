cmake_minimum_required(VERSION 3.28)
project(WinAgent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON) # Qt'nin Meta-Object Compiler'ını açar
set(CMAKE_AUTOUIC ON) # .ui dosyaları kullanacaksan açar
set(CMAKE_AUTORCC ON) # .qrc (kaynak) dosyaları için açar

# 1. Qt Paketlerini Bul (Widgets ve Network gerekiyor)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

include_directories(include)

# 3. Kaynak Dosyaları
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

# 4. Kütüphane Bağlantıları (Qt kütüphanelerini ekledik)
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

# 5. Windows Uygulama Tipi
# Eğer uygulamanın arkada bir konsol penceresi açmasını İSTEMİYORSAN:
# set_target_properties(WinAgent PROPERTIES WIN32_EXECUTABLE TRUE)

add_compile_definitions(WIN32_LEAN_AND_MEAN)