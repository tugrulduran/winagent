#pragma once

#include "BaseMonitor.h"

class DummyMonitor : public BaseMonitor {
public:
    DummyMonitor() : BaseMonitor(10) {}

    void init() override {}
    void update() override {}
};
