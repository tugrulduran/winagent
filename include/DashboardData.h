#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_set>

struct CPUData {
private:
    std::atomic<float> load{0.0f};
    std::atomic<uint32_t> cores{0};

public:
    void setLoad(const float value) { load.store(value, std::memory_order_relaxed); }
    void setCores(const uint32_t value) { cores.store(value, std::memory_order_relaxed); }

    float getLoad() const { return load.load(std::memory_order_relaxed); }
    uint32_t getCores() const { return cores.load(std::memory_order_relaxed); }
};

struct MemoryData {
private:
    std::atomic<uint64_t> used{0};
    std::atomic<uint64_t> total{0};

public:
    void setUsed(const uint64_t value) { used.store(value, std::memory_order_relaxed); }
    void setTotal(const uint64_t value) { total.store(value, std::memory_order_relaxed); }

    uint64_t getUsed() const { return used.load(std::memory_order_relaxed); }
    uint64_t getTotal() const { return total.load(std::memory_order_relaxed); }
};

struct NetworkData {
private:
    std::atomic<uint64_t> rxBytes{0};
    std::atomic<uint64_t> txBytes{0};

public:
    void setRxBytes(const uint64_t value) { rxBytes.store(value, std::memory_order_relaxed); }
    void setTxBytes(const uint64_t value) { txBytes.store(value, std::memory_order_relaxed); }

    uint64_t getRxBytes() const { return rxBytes.load(std::memory_order_relaxed); }
    uint64_t getTxBytes() const { return txBytes.load(std::memory_order_relaxed); }
};

struct AudezeData {
private:
    std::atomic<uint8_t> battery{0};

public:
    void setBattery(const uint8_t value) { battery.store(value, std::memory_order_relaxed); }

    uint8_t getBattery() const { return battery.load(std::memory_order_relaxed); }
};

struct AppAudioData {
    std::atomic<uint64_t> pid{0};
    std::wstring name;
    mutable std::mutex nameMutex;
    std::atomic<float> volume{0};
    std::atomic<bool> muted{false};
};

struct AppAudioSnapshot {
    uint64_t pid;
    std::wstring name;
    float volume;
    bool muted;
};

struct AudioApps {
private:
    std::vector<std::unique_ptr<AppAudioData> > list;
    mutable std::mutex mutex;

public:
    std::vector<AppAudioSnapshot> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);

        std::vector<AppAudioSnapshot> out;
        out.reserve(list.size());

        for (const auto &app: list) {
            AppAudioSnapshot snap;
            snap.pid = app->pid.load(std::memory_order_relaxed);
            snap.volume = app->volume.load(std::memory_order_relaxed);
            snap.muted = app->muted.load(std::memory_order_relaxed); {
                std::lock_guard<std::mutex> nameLock(app->nameMutex);
                snap.name = app->name;
            }

            out.push_back(std::move(snap));
        }

        return out;
    }

    void add(const uint64_t pid, const std::wstring &name, const float volume, const bool muted) {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto& app : list) {
            if (app->pid.load(std::memory_order_relaxed) == pid) {
                app->volume.store(volume, std::memory_order_relaxed);
                app->muted.store(muted, std::memory_order_relaxed);

                {
                    std::lock_guard<std::mutex> nameLock(app->nameMutex);
                    app->name = name;
                }
                return;
            }
        }

        // yoksa ekle
        auto app = std::make_unique<AppAudioData>();
        app->pid.store(pid, std::memory_order_relaxed);
        app->volume.store(volume, std::memory_order_relaxed);
        app->muted.store(muted, std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> nameLock(app->nameMutex);
            app->name = name;
        }

        list.emplace_back(std::move(app));
    }

    void remove(const uint64_t pid) {
        std::lock_guard<std::mutex> lock(mutex);

        std::erase_if(list, [&](const std::unique_ptr<AppAudioData> &app) {
            return app->pid.load(std::memory_order_relaxed) == pid;
        });
    }

    void removeMissing(const std::unordered_set<uint64_t>& alive)
    {
        std::lock_guard<std::mutex> lock(mutex);

        std::erase_if(list, [&](const std::unique_ptr<AppAudioData>& app) {
            if (app->pid == 0xFFFFFFFF) return false;
            return !alive.contains(app->pid.load(std::memory_order_relaxed));
        });
    }

    void setVolume(const uint64_t pid, const float volume) {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto &app: list) {
            if (app->pid.load(std::memory_order_relaxed) == pid) {
                app->volume.store(volume, std::memory_order_relaxed);
                return;
            }
        }
    }

    void setMuted(const uint64_t pid, const bool muted) {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto &app: list) {
            if (app->pid.load(std::memory_order_relaxed) == pid) {
                app->muted.store(muted, std::memory_order_relaxed);
                return;
            }
        }
    }

    void setName(const uint64_t pid, const std::wstring &name) {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto &app: list) {
            if (app->pid.load(std::memory_order_relaxed) == pid) {
                std::lock_guard<std::mutex> nameLock(app->nameMutex);
                app->name = name;
                return;
            }
        }
    }
};

struct AudioDeviceData {
    std::atomic<uint32_t> index{0};
    std::wstring name;
    mutable std::mutex nameMutex;
    std::wstring deviceId;
    mutable std::mutex deviceIdMutex;
    std::atomic<bool> isDefault{false};
};

struct AudioDeviceSnapshot {
    uint32_t index;
    std::wstring name;
    std::wstring deviceId;
    bool isDefault;
};

struct AudioDevices {
private:
    std::vector<std::unique_ptr<AudioDeviceData> > list;
    mutable std::mutex mutex;

public:
    std::vector<AudioDeviceSnapshot> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex);

        std::vector<AudioDeviceSnapshot> out;
        out.reserve(list.size());

        for (const auto &device: list) {
            AudioDeviceSnapshot snap;
            snap.index = device->index.load(std::memory_order_relaxed);
            snap.isDefault = device->isDefault.load(std::memory_order_relaxed); {
                std::lock_guard<std::mutex> nameLock(device->nameMutex);
                snap.name = device->name;
            } {
                std::lock_guard<std::mutex> nameLock(device->deviceIdMutex);
                snap.deviceId = device->deviceId;
            }

            out.push_back(std::move(snap));
        }

        return out;
    }

    void add(const uint32_t index, const std::wstring &name, const std::wstring &deviceId, const bool isDefault) {
        std::lock_guard<std::mutex> lock(mutex);

        for (const auto &dev: list) {
            if (dev->index.load(std::memory_order_relaxed) == index) {
                dev->isDefault.store(isDefault, std::memory_order_relaxed); {
                    std::lock_guard<std::mutex> nameLock(dev->nameMutex);
                    dev->name = name;
                } {
                    std::lock_guard<std::mutex> idLock(dev->deviceIdMutex);
                    dev->deviceId = deviceId;
                }
                return;
            }
        }

        auto dev = std::make_unique<AudioDeviceData>();
        dev->index.store(index, std::memory_order_relaxed);
        dev->isDefault.store(isDefault, std::memory_order_relaxed); {
            std::lock_guard<std::mutex> nameLock(dev->nameMutex);
            dev->name = name;
        } {
            std::lock_guard<std::mutex> idLock(dev->deviceIdMutex);
            dev->deviceId = deviceId;
        }

        list.emplace_back(std::move(dev));
    }

    void remove(const uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex);

        std::erase_if(list, [&](const std::unique_ptr<AudioDeviceData> &dev) {
            return dev->index.load(std::memory_order_relaxed) == index;
        });
    }

    void removeMissing(const std::unordered_set<uint32_t>& alive)
    {
        std::lock_guard<std::mutex> lock(mutex);

        std::erase_if(list, [&](const std::unique_ptr<AudioDeviceData>& dev) {
            return !alive.contains(dev->index.load(std::memory_order_relaxed));
        });
    }

    void setDefault(const uint32_t index) {
        std::lock_guard<std::mutex> lock(mutex);

        for (auto &dev: list) {
            const bool isDef = dev->index.load(std::memory_order_relaxed) == index;
            dev->isDefault.store(isDef, std::memory_order_relaxed);
        }
    }
};

struct AudioData {
    AudioApps apps;
    AudioDevices devices;
};

struct SysProcess {
    uint32_t id;
    std::string name;
};

struct ProcessData {
private:
    SysProcess activeProcess = {0, ""};
    mutable std::mutex activeProcessMutex;

public:
    void setActiveProcess(const uint32_t id, const std::string &name) {
        std::lock_guard<std::mutex> lock(activeProcessMutex);
        activeProcess.id = id;
        activeProcess.name = name;
    }

    SysProcess getActiveProcess() const {
        std::lock_guard<std::mutex> lock(activeProcessMutex);
        return activeProcess;
    }
};

struct LauncherAction {
    uint32_t id;
    std::wstring name;
    std::string execPath;
};

struct LauncherData {
private:
    std::vector<LauncherAction> list;

public:
    std::vector<LauncherAction> getActions() const {
        std::vector<LauncherAction> out;
        out.reserve(list.size());

        for (const auto &item: list) {
            LauncherAction launcher;
            launcher.id = item.id;
            launcher.name = item.name;
            launcher.execPath = item.execPath;

            out.push_back(std::move(launcher));
        }

        return out;
    }

    void add(uint32_t id, const std::wstring &name, const std::string &execPath) {
        LauncherAction item;
        item.id = id;
        item.name = name;
        item.execPath = execPath;
        list.push_back(item);
    }
};

enum MediaSource : uint8_t {
    MEDIA_SOURCE_NO_MEDIA = 0,
    MEDIA_SOURCE_GENERIC,
    MEDIA_SOURCE_YOUTUBE,
    MEDIA_SOURCE_TWITCH,
    MEDIA_SOURCE_KICK,
    MEDIA_SOURCE_MEDIA_PLAYER,
    MEDIA_SOURCE_YOUTUBE_MUSIC,
    MEDIA_SOURCE_SPOTIFY
};

struct MediaData {
    std::wstring title;
    mutable std::mutex titleMutex;

    std::atomic<uint8_t> source{MEDIA_SOURCE_NO_MEDIA};
    std::atomic<uint64_t> duration{0};
    std::atomic<uint64_t> currentTime{0};
    std::atomic<bool> isPlaying{false};

    void set(const std::wstring &newTitle, const uint8_t newSource, const uint64_t newDuration,
             const uint64_t newCurrentTime, const bool playing) { {
            std::lock_guard<std::mutex> lock(titleMutex);
            title = newTitle;
        }

        source.store(newSource, std::memory_order_relaxed);
        duration.store(newDuration, std::memory_order_relaxed);
        currentTime.store(newCurrentTime, std::memory_order_relaxed);
        isPlaying.store(playing, std::memory_order_relaxed);
    }

    void clear() { {
            std::lock_guard<std::mutex> lock(titleMutex);
            title.clear();
        }

        source.store(MEDIA_SOURCE_NO_MEDIA, std::memory_order_relaxed);
        duration.store(0, std::memory_order_relaxed);
        currentTime.store(0, std::memory_order_relaxed);
        isPlaying.store(false, std::memory_order_relaxed);
    }
};

struct DashboardData {
    CPUData cpu;
    MemoryData memory;
    NetworkData network;
    AudezeData audeze;
    AudioData audio;
    MediaData media;
    ProcessData process;
    LauncherData launcher;
};
