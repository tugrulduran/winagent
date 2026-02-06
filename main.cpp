#include <iostream>
#include <memory>
#include <conio.h>
#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "NetworkReporter.h"
#include "modules/AudioMonitor.h"

// Senin orijinal fonksiyonun
void HideCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &cursorInfo);
}

// Pencereyi sadece her zaman üstte tutar (Transparanlık kaldırıldı)
void SetAlwaysOnTop() {
    HWND hwnd = GetConsoleWindow();
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowTextA(hwnd, "WinAgent v1.3 | CONTROL CENTER");
}

void ResetCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = { 0, 0 };
    SetConsoleCursorPosition(hOut, coord);
}

int main() {
    HideCursor();      // Geri geldi
    SetAlwaysOnTop();  // Sadece üstte tutma aktif
    system("cls");

    auto cpu = std::make_unique<CPUMonitor>(1000);
    auto ram = std::make_unique<MemoryMonitor>(1000);
    auto net = std::make_unique<NetworkBytes>(1000);
    auto audio = std::make_unique<AudioMonitor>(1000);

    auto reporter = std::make_unique<NetworkReporter>(
        "127.0.0.1", 5000, 1000,
        cpu.get(), ram.get(), net.get(), audio.get()
    );

    cpu->start();
    ram->start();
    net->start();
    audio->start();
    reporter->start();

    bool nodeRunning = false;

    while (true) {
        ResetCursor();

        std::cout << "==========================================" << std::endl;
        std::cout << "   WINAGENT v1.3 - SYSTEM COMMAND CENTER  " << std::endl;
        std::cout << "==========================================" << std::endl;

        cpu->display();
        ram->display();
        audio->display();

        std::cout << "------------------------------------------" << std::endl;
        std::cout << " [S] NODE.JS SERVER: " << (nodeRunning ? "[RUNNING]" : "[STOPPED]") << std::endl;
        std::cout << " [Q] EXIT AGENT" << std::endl;
        std::cout << "------------------------------------------" << std::endl;

        // Klavye kontrolü
        if (_kbhit()) {
            char key = _getch();
            if (key == 's' || key == 'S') {
                if (!nodeRunning) {
                    // Node.js'i nodedash klasöründe başlat
                    system("start node ../../nodedash/server.js");
                    nodeRunning = true;
                } else {
                    system("taskkill /f /im node.exe >nul 2>&1");
                    nodeRunning = false;
                }
            }
            if (key == 'q' || key == 'Q') break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    system("taskkill /f /im node.exe >nul 2>&1");
    return 0;
}