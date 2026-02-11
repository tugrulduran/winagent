#pragma once

#include "DashboardData.h"

class Dashboard {
public:
    static Dashboard& instance() {
        static Dashboard instance;
        return instance;
    }

    DashboardData data;

private:
    Dashboard() = default;
};
