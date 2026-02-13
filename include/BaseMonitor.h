#ifndef BASEMONITOR_H
#define BASEMONITOR_H

#include <thread>
#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "Dashboard.h"

enum class ThreadState {
    Stopped,
    Starting,
    Running,
    Stopping
};

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
    std::atomic<ThreadState> state{ThreadState::Stopped};

    // ms = update interval in milliseconds.
    BaseMonitor(int ms) : intervalMs(ms), dashboard_(nullptr) {
    }

    BaseMonitor(int ms, Dashboard &dashboard) : intervalMs(ms), dashboard_(&dashboard) {
    }

    virtual ~BaseMonitor() {
        stop();
        assert(state.load() == ThreadState::Stopped);
    }

    // Called once at start(). Use this to create OS handles, queries, etc.
    virtual void init() = 0;

    // Called repeatedly by the worker thread. Must be fast and safe.
    virtual void update() = 0;

    // Starts the worker thread. Safe to call once; repeated calls do nothing.
    void start() {
        std::lock_guard<std::mutex> lock(lifecycle_m);

        if (state.load() != ThreadState::Stopped) { return; }

        state.store(ThreadState::Starting);

        try {
            init();
        } catch (...) {
            // @todo: log this exception
            state.store(ThreadState::Stopped);
            throw;
        }

        active = true;
        state.store(ThreadState::Running);

        workerThread = std::thread([this] {
            try {
                run();
            } catch (...) {
                // @todo: log this exception
            }

            active = false;
        });
    }

    // Stops the worker thread and waits for it to finish.
    void stop() {
        std::lock_guard<std::mutex> lock(lifecycle_m);

        if (state.load() == ThreadState::Stopped) { return; }

        active = false;
        cv.notify_all();

        if (workerThread.joinable())
            workerThread.join();

        state.store(ThreadState::Stopped);
    }

protected:
    std::atomic<bool> active{false};
    std::thread workerThread;
    std::condition_variable cv; // Eklendi
    std::mutex cv_m; // Eklendi
    std::mutex lifecycle_m;
    int intervalMs;
    Dashboard *dashboard_;

    // Worker loop. Derived classes normally do not override this.
    virtual void run() {
        while (active) {
            try {
                update();
            } catch (...) {
                // @todo: log this exception
            }

            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait_for(lk, std::chrono::milliseconds(intervalMs), [this] {
                return !active;
            });
        }
    }

    bool isActive() const noexcept {
        return active.load(std::memory_order_acquire);
    }
};

#endif
