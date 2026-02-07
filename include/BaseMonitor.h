#ifndef BASEMONITOR_H
#define BASEMONITOR_H

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

class BaseMonitor {
protected:
    std::atomic<bool> active{false};
    std::thread workerThread;
    int intervalMs;

    // Arka planda sürekli çalışacak olan fonksiyon
    virtual void run() {
        while (active) {
            update();
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

public:
    BaseMonitor(int ms) : intervalMs(ms) {}
    virtual ~BaseMonitor() { stop(); }

    virtual void update() = 0;
    virtual void display() const = 0;

    void start() {
        if (!active) {
            active = true;
            // Küçük bir gecikme nesnenin bellekte tam oturmasını sağlar
            workerThread = std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                this->run();
            });
        }
    }
    void stop() {
        active = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
};

#endif