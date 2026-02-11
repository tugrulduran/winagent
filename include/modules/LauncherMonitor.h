#ifndef LAUNCHERMONITOR_H
#define LAUNCHERMONITOR_H

#include <vector>
#include "BaseMonitor.h"

class LauncherMonitor : public BaseMonitor {
public:
    LauncherMonitor(int interval, Dashboard &dashboard) : BaseMonitor(interval, dashboard) {
    }

    ~LauncherMonitor() override { stop(); }

    void init() override;

    void update() override {
    }

    void executeAction(uint32_t id);

private:
    std::vector<LauncherAction> actions = {
        {1, L"Calculator", "calc.exe"},
        {2, L"Notepad", "notepad.exe"},
        {3, L"Chrome", "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"},
        {4, L"Steam", "C:\\Program Files (x86)\\Steam\\steam.exe"},
        {5, L"Powershell", "powershell.exe"}
    };
};
#endif
