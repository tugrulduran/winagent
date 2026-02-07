#include "modules/AudioDeviceMonitor.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <propvarutil.h>

#include <vector>
#include <string>
#include <mutex>
#include <iostream>

// =======================================================
//  GLOBAL / STATIC
// =======================================================

std::mutex AudioDeviceMonitor::deviceMutex;
typedef LONGLONG REFERENCE_TIME;

// =======================================================
//  IPolicyConfigVista (STABLE / WORKING VERSION)
// =======================================================

struct __declspec(uuid("568b9108-44bf-40b4-9006-86afe5b5a620")) IPolicyConfigVista;

interface IPolicyConfigVista : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(
        PCWSTR, WAVEFORMATEX**) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(
        PCWSTR, BOOL, WAVEFORMATEX**) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(
        PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(
        PCWSTR, BOOL, REFERENCE_TIME*, REFERENCE_TIME*) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(
        PCWSTR, REFERENCE_TIME*) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetShareMode(
        PCWSTR, void*) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetShareMode(
        PCWSTR, void*) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(
        PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(
        PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(
        PCWSTR deviceId, ERole role) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(
        PCWSTR, BOOL) = 0;
};

// =======================================================
//  CLSID (WINDOWS 10 / 11)
// =======================================================

const CLSID CLSID_CPolicyConfigClient =
{ 0x870af99c, 0x171d, 0x4f9e, {0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xd9} };

// =======================================================
//  GET AUDIO DEVICES
// =======================================================

std::vector<AudioDeviceInfo> AudioDeviceMonitor::getDevices()
{
    std::lock_guard<std::mutex> lock(deviceMutex);

    std::vector<AudioDeviceInfo> deviceList;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator
    );

    if (FAILED(hr) || !enumerator)
        return deviceList;

    IMMDeviceCollection* collection = nullptr;
    enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);

    IMMDevice* defaultDev = nullptr;
    enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDev);

    LPWSTR defaultId = nullptr;
    if (defaultDev)
        defaultDev->GetId(&defaultId);

    UINT count = 0;
    collection->GetCount(&count);

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        collection->Item(i, &device);

        IPropertyStore* props = nullptr;
        device->OpenPropertyStore(STGM_READ, &props);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        props->GetValue(PKEY_Device_FriendlyName, &varName);

        LPWSTR id = nullptr;
        device->GetId(&id);

        AudioDeviceInfo info;
        info.index = static_cast<int>(i);
        info.name = varName.pwszVal ? varName.pwszVal : L"Bilinmeyen";
        info.deviceId = id ? id : L"";
        info.isDefault = (defaultId && id && wcscmp(id, defaultId) == 0);

        deviceList.push_back(info);

        PropVariantClear(&varName);
        if (id) CoTaskMemFree(id);
        props->Release();
        device->Release();
    }

    if (defaultId) CoTaskMemFree(defaultId);
    if (defaultDev) defaultDev->Release();

    collection->Release();
    enumerator->Release();

    CoUninitialize();
    return deviceList;
}

// =======================================================
//  SET DEFAULT DEVICE
// =======================================================

void AudioDeviceMonitor::setDefaultDevice(int index)
{
    auto devices = getDevices();
    if (index < 0 || index >= (int)devices.size())
        return;

    std::lock_guard<std::mutex> lock(deviceMutex);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IPolicyConfigVista* pPolicyConfig = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_CPolicyConfigClient,
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IPolicyConfigVista),
        (void**)&pPolicyConfig
    );

    if (SUCCEEDED(hr) && pPolicyConfig) {

        const wchar_t* id = devices[index].deviceId.c_str();

        pPolicyConfig->SetDefaultEndpoint(id, eMultimedia);
        pPolicyConfig->SetDefaultEndpoint(id, eConsole);
        pPolicyConfig->SetDefaultEndpoint(id, eCommunications);

        std::wcout << L"[SYSTEM] Varsayılan ses aygıtı değiştirildi: "
                   << devices[index].name << std::endl;

        pPolicyConfig->Release();
    }
    else {
        std::cerr << "[ERROR] Default device değiştirilemedi. HRESULT=0x"
                  << std::hex << hr << std::endl;
    }

    CoUninitialize();
}
