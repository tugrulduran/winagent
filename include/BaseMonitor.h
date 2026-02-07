#ifndef BASEMONITOR_H
#define BASEMONITOR_H

#include <thread>
#include <atomic>
#include <chrono>

class BaseMonitor {
protected:
    std::atomic<bool> active{false};
    std::thread workerThread;
    int intervalMs;

    virtual void run() {
        while (active) {
            update();
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

public:
    BaseMonitor(int ms) : intervalMs(ms) {}
    virtual ~BaseMonitor() { stop(); }

    virtual void init() = 0;
    virtual void update() = 0;

    void start() {
        if (!active) {
            init();
            active = true;
            workerThread = std::thread(&BaseMonitor::run, this);
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