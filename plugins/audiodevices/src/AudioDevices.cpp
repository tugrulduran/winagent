#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <QJsonArray>
#include "AudioDevices.h"

#pragma comment(lib, "ole32.lib")


// ============================================================
// IPolicyConfig (undocumented interface)
// Important:
// - The method order must match the real Windows interface layout.
// - Do not reorder methods unless you know exactly what you are doing.
// ============================================================
interface IPolicyConfig : public IUnknown {
public:
    virtual HRESULT GetMixFormat(PCWSTR, WAVEFORMATEX **) = 0;

    virtual HRESULT GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX **) = 0;

    virtual HRESULT ResetDeviceFormat(PCWSTR) = 0;

    virtual HRESULT SetDeviceFormat(PCWSTR, WAVEFORMATEX *, WAVEFORMATEX *) = 0;

    virtual HRESULT GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;

    virtual HRESULT SetProcessingPeriod(PCWSTR, PINT64) = 0;

    virtual HRESULT GetShareMode(PCWSTR, void *) = 0;

    virtual HRESULT SetShareMode(PCWSTR, void *) = 0;

    virtual HRESULT GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;

    virtual HRESULT SetPropertyValue(PCWSTR, const PROPERTYKEY &, const PROPVARIANT *) = 0;

    virtual HRESULT SetDefaultEndpoint(PCWSTR, ERole) = 0;

    virtual HRESULT SetEndpointVisibility(PCWSTR, BOOL) = 0;
};

static const CLSID CLSID_PolicyConfigClient = {0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};

static const IID IID_IPolicyConfig = {0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

// RAII helper for COM initialization.
class ScopedCOM {
public:
    ScopedCOM() { initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)); }
    ~ScopedCOM() { if (initialized) CoUninitialize(); }

private:
    bool initialized{false};
};

namespace audiodevices {
    void AudioDevices::init(QJsonObject config) {
        const QJsonValue v = config.value("ignoredDevices");
        ignoredDevices.clear();
        if (!v.isArray()) {
            return;
        }

        const QJsonArray arr = v.toArray();
        for (const QJsonValue &item: arr) {
            if (!item.isString()) continue;

            const QString s = item.toString().trimmed();
            if (s.isEmpty()) continue;

            ignoredDevices.insert(s.toStdWString());
        }
    }

    std::vector<AudioDevice> AudioDevices::getDevices() {
        auto devices = listDevices();

        return devices;
    }


    bool AudioDevices::isIgnored(const std::wstring &name) {
        // Simple substring match against ignoreList.
        for (const auto &ignored: ignoredDevices) {
            if (name.find(ignored) != std::wstring::npos) return true;
        }
        return false;
    }

    bool AudioDevices::setDefaultByIndex(int index) {
        // Convenience helper: pick deviceId from listDevices() by index.
        auto devices = listDevices();
        if (index < 0 || index >= (int) devices.size()) return false;
        return setDefaultById(devices[index].deviceId);
    }

    std::vector<AudioDevice> AudioDevices::listDevices() {
        // Enumerates active render devices and marks the default one.
        std::vector<AudioDevice> devices;
        ScopedCOM com;

        IMMDeviceEnumerator *enumerator = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **) &enumerator);
        if (FAILED(hr)) return devices;

        IMMDevice *defaultDevice = nullptr;
        LPWSTR defaultId = nullptr;
        enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
        if (defaultDevice) defaultDevice->GetId(&defaultId);

        IMMDeviceCollection *collection = nullptr;
        enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        if (!collection) {
            if (defaultId) CoTaskMemFree(defaultId);
            if (defaultDevice) defaultDevice->Release();
            enumerator->Release();
            return devices;
        }

        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            IMMDevice *device = nullptr;
            if (FAILED(collection->Item(i, &device))) continue;

            IPropertyStore *props = nullptr;
            device->OpenPropertyStore(STGM_READ, &props);

            PROPVARIANT name;
            PropVariantInit(&name);
            if (props) props->GetValue(PKEY_Device_FriendlyName, &name);

            LPWSTR id = nullptr;
            device->GetId(&id);

            devices.push_back({
                (int) i,
                name.pwszVal ? name.pwszVal : L"Unknown Device",
                id ? id : L"",
                (defaultId && id && wcscmp(defaultId, id) == 0)
            });

            if (id) CoTaskMemFree(id);
            PropVariantClear(&name);
            if (props) props->Release();
            device->Release();
        }

        if (defaultId) CoTaskMemFree(defaultId);
        if (defaultDevice) defaultDevice->Release();
        collection->Release();
        enumerator->Release();
        return devices;
    }

    bool AudioDevices::setDefaultById(const std::wstring &deviceId) {
        ScopedCOM com;
        IPolicyConfig *policy = nullptr;

        HRESULT hr = CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_INPROC_SERVER, IID_IPolicyConfig, (void **) &policy);
        if (FAILED(hr)) return false;

        policy->SetDefaultEndpoint(deviceId.c_str(), eConsole);
        policy->SetDefaultEndpoint(deviceId.c_str(), eMultimedia);
        policy->SetDefaultEndpoint(deviceId.c_str(), eCommunications);

        policy->Release();
        return true;
    }
}
