#pragma once
#include "BaseMonitor.h"
#include <stdexcept>

class ExceptionMonitor : public BaseMonitor {
public:
    ExceptionMonitor()
        : BaseMonitor(1) {}   // 1ms interval

protected:
    void init() override {}

    void update() override {
        throw std::runtime_error("boom");
    }
};
