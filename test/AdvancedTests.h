#pragma once
#include "BaseMonitor.h"
#include <chrono>
#include <thread>
#include <atomic>

// -----------------------------

class InitThrowMonitor : public BaseMonitor {
public:
    InitThrowMonitor() : BaseMonitor(10) {}

    void init() override {
        throw std::runtime_error("init failed");
    }

    void update() override {}
};

// -----------------------------

class SlowInitMonitor : public BaseMonitor {
public:
    SlowInitMonitor() : BaseMonitor(10) {}

    void init() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void update() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

// -----------------------------

class CounterMonitor : public BaseMonitor {
public:
    CounterMonitor() : BaseMonitor(1) {}

    std::atomic<int> counter{0};

    void init() override {}

    void update() override {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
};

// -----------------------------

class UpdateThrowMonitor : public BaseMonitor {
public:
    UpdateThrowMonitor() : BaseMonitor(1) {}

    void init() override {}

    void update() override {
        throw std::runtime_error("boom");
    }
};

class VerySlowUpdateMonitor : public BaseMonitor {
public:
    VerySlowUpdateMonitor() : BaseMonitor(10) {}

    void init() override {}

    void update() override {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
};
