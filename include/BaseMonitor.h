#ifndef BASEMONITOR_H
#define BASEMONITOR_H

#include <thread>
#include <atomic>
#include <chrono>

#include "Dashboard.h"

/*
 * BaseMonitor
 * ----------
 * A tiny framework for "monitor" modules.
 *
 * Idea:
 * - Each monitor runs in its own worker thread.
 * - The thread calls update(), then sleeps for intervalMs milliseconds.
 * - init() runs once when start() is called for the first time.
 *
 * Thread-safety rule:
 * - update() writes new data.
 * - getData() (in derived classes) should lock a mutex and return a copy.
 */
class BaseMonitor {
public:
    // ms = update interval in milliseconds.
    BaseMonitor(int ms)
        : intervalMs(ms), dashboard_(nullptr) {}

    BaseMonitor(int ms, Dashboard& dashboard)
        : intervalMs(ms), dashboard_(&dashboard) {}

    virtual ~BaseMonitor() { stop(); }

    // Called once at start(). Use this to create OS handles, queries, etc.
    virtual void init() = 0;

    // Called repeatedly by the worker thread. Must be fast and safe.
    virtual void update() = 0;

    // Starts the worker thread. Safe to call once; repeated calls do nothing.
    void start() {
        if (!active) {
            init();
            active = true;
            workerThread = std::thread(&BaseMonitor::run, this);
        }
    }

    // Stops the worker thread and waits for it to finish.
    void stop() {
        if (active) {
            active = false;
            cv.notify_all(); // Thread uykudaysa hemen uyandır!

            if (workerThread.joinable()) {
                workerThread.join();
            }
        }
    }
protected:
    std::atomic<bool> active{false};
    std::thread workerThread;
    std::condition_variable cv; // Eklendi
    std::mutex cv_m;            // Eklendi
    int intervalMs;
    Dashboard* dashboard_;

    // Worker loop. Derived classes normally do not override this.
    virtual void run() {
        while (active) {
            update();

            // Sabit uyku yerine, 'active' değişene veya süre dolana kadar bekle
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait_for(lk, std::chrono::milliseconds(intervalMs), [this]{
                return !active; // active false olursa hemen uyanır
            });
        }
    }};

#endif