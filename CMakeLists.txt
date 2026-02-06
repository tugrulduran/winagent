cmake_minimum_required(VERSION 3.28)
project(WinAgent)

set(CMAKE_CXX_STANDARD 20)

# Statik bağlama bayrakları (DLL hatalarını çözer)
if (MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif ()

# Header dosyalarının yerini göster
include_directories(include)

# Yürütülebilir dosyayı tanımla
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
        src/modules/AudioMonitor.cpp)

# Windows sistem kütüphanelerini projeye dahil et
# psapi: Süreç (process) bilgileri için
# pdh: Performans sayaçları (CPU kullanımı vb.) için
target_link_libraries(WinAgent PRIVATE
        psapi
        pdh
        ws2_32
        iphlpapi
        kernel32
        user32
        ole32
        shlwapi
        version
)