#pragma once
#include <string>
#include <vector>

struct AudioDevice {
    int index{};
    std::wstring name;
    std::wstring deviceId;
    bool isDefault{};
};

class AudioDeviceSwitcher {
public:
    // Statik metodlar kalsın, çünkü her an veri toplamazlar, sadece çağrıldığında çalışırlar.
    static std::vector<AudioDevice> listDevices();
    static bool setDefaultById(const std::wstring& deviceId);
    static bool setDefaultByIndex(int index);
};