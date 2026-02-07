#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <memory>
#include <conio.h>
#include <thread>

#include "modules/CPUMonitor.h"
#include "modules/MemoryMonitor.h"
#include "modules/NetworkBytes.h"
#include "NetworkReporter.h"
#include "modules/AudioMonitor.h"
#include "modules/MediaMonitor.h"

void HideCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &cursorInfo);
}

void ResetCursor() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = { 0, 0 };
    SetConsoleCursorPosition(hOut, coord);
}

int main() {
    SetConsoleOutputCP(65001); // UTF-8 Karakter desteği
    HideCursor();
    system("cls");

    // Modüllerin Oluşturulması
    auto cpu = std::make_unique<CPUMonitor>(1000);
    auto ram = std::make_unique<MemoryMonitor>(1000);
    auto net = std::make_unique<NetworkBytes>(1000);
    auto audio = std::make_unique<AudioMonitor>(1000);
    auto media = std::make_unique<MediaMonitor>(1000);

    // Reporter (Dashboard'a veri gönderen ve komut dinleyen)
    auto reporter = std::make_unique<NetworkReporter>(
        "127.0.0.1", 5000, 1000,
        cpu.get(), ram.get(), net.get(), audio.get(), media.get()
    );

    // Modülleri Başlat
    cpu->start();
    ram->start();
    net->start();
    media->start();
    audio->start();
    reporter->start();

    bool nodeRunning = false;

    while (true) {
        ResetCursor();
        std::cout << "==========================================" << std::endl;
        std::cout << "   WINAGENT v1.4 - MSVC CONTROL CENTER   " << std::endl;
        std::cout << "==========================================" << std::endl;

        cpu->display();
        ram->display();
        audio->display();
        media->display();

        std::cout << "------------------------------------------" << std::endl;
        std::cout << " [S] NODE.JS SERVER: " << (nodeRunning ? "[RUNNING]" : "[STOPPED]") << std::endl;
        std::cout << " [Q] EXIT AGENT" << std::endl;
        std::cout << "------------------------------------------" << std::endl;

        if (_kbhit()) {
            char key = _getch();
            if (key == 's' || key == 'S') {
                if (!nodeRunning) {
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

    // Programdan çıkış: Tüm thread'leri güvenli şekilde durdur
    cpu->stop();
    ram->stop();
    net->stop();
    media->stop();
    audio->stop();
    reporter->stop();

    if (nodeRunning) system("taskkill /f /im node.exe >nul 2>&1");

    // İmleci geri getir
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hOut, &cursorInfo);
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hOut, &cursorInfo);

    std::cout << "\nAgent temiz bir sekilde kapatildi." << std::endl;
    return 0;
}