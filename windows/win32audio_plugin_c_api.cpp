#include "include/win32audio/win32audio_plugin_c_api.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <ole2.h>
#include <ShellAPI.h>
#include <olectl.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propvarutil.h>
#include <stdio.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <string>
#include <vector>
#include "include/Policyconfig.h"
#include "include/encoding.h"
#include <endpointvolume.h>
// #include "hicon_to_bytes.cpp"

#pragma warning(push)
#pragma warning(disable : 4201)
#include "hicon_to_bytes.cpp"
// #pragma warning(disable: 4201)
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma warning(pop)

#pragma comment(lib, "ole32")
#pragma comment(lib, "propsys")
using namespace std;

// #pragma warning(disable: 4244)

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/encodable_value.h>

#include <memory>
#include <sstream>

#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <propvarutil.h>

struct ProcessVolume
{
    int processId = 0;
    std::string processPath = "";
    float maxVolume = 1.0;
    float peakVolume = 0.0;
};

struct DeviceProps
{
    wstring id;
    wstring name;
    wstring iconInfo;
    bool isActive;
};

static HRESULT getDeviceProperty(IMMDevice *pDevice, DeviceProps *pOutput)
{
    if (pDevice == nullptr || pOutput == nullptr)
    {
        return E_POINTER;
    }

    IPropertyStore *pPropertyStore = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
    if (FAILED(hr))
    {
        return hr;
    }

    PROPVARIANT varName = {0};
    PropVariantInit(&varName);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &varName);
    if (SUCCEEDED(hr))
    {
        if (IsPropVariantString(varName))
        {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varName, &strret)))
            {
                pOutput->name = strret.pOleStr;
            }
            else
            {
                hr = E_UNEXPECTED;
            }
        }
        else
        {
            hr = E_UNEXPECTED;
        }

        PropVariantClear(&varName);
    }

    PROPVARIANT varIconPath = {0};
    PropVariantInit(&varIconPath);
    hr = pPropertyStore->GetValue(PKEY_DeviceClass_IconPath, &varIconPath);
    if (SUCCEEDED(hr))
    {
        if (IsPropVariantString(varIconPath))
        {
            STRRET strret;
            if (SUCCEEDED(PropVariantToStrRet(varIconPath, &strret)))
            {
                pOutput->iconInfo = strret.pOleStr;
            }
            else
            {
                pOutput->iconInfo = L"missing,0";
                hr = E_UNEXPECTED;
            }
        }
        else
        {
            pOutput->iconInfo = L"missing,0";
            hr = E_UNEXPECTED;
        }

        PropVariantClear(&varIconPath);
    }

    pPropertyStore->Release();

    return hr;
}

std::vector<DeviceProps> EnumAudioDevices(EDataFlow deviceType, ERole eRole)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to initialize COM\n");
        return output;
    }

    IMMDeviceEnumerator *pEnumerator = nullptr;
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
    if (FAILED(hr) || !pEnumerator)
    {
        OutputDebugString(L"Failed to create device enumerator\n");
        CoUninitialize();
        return output;
    }

    IMMDevice *pActive = nullptr;
    wstring activeDevID;

    hr = pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
    if (SUCCEEDED(hr) && pActive)
    {
        LPWSTR activeID = nullptr;
        hr = pActive->GetId(&activeID);
        if (SUCCEEDED(hr) && activeID)
        {
            activeDevID = activeID;
            CoTaskMemFree(activeID);
        }
        pActive->Release();
    }

    IMMDeviceCollection *pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(deviceType, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr) || !pCollection)
    {
        OutputDebugString(L"Failed to enumerate audio endpoints\n");
        if (pEnumerator)
            pEnumerator->Release();
        CoUninitialize();
        return output;
    }

    UINT cEndpoints = 0;
    hr = pCollection->GetCount(&cEndpoints);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to get count of audio endpoints\n");
        pCollection->Release();
        pEnumerator->Release();
        CoUninitialize();
        return output;
    }

    for (UINT n = 0; n < cEndpoints; ++n)
    {
        IMMDevice *pDevice = nullptr;
        hr = pCollection->Item(n, &pDevice);
        if (FAILED(hr) || !pDevice)
        {
            OutputDebugString(L"Failed to get audio endpoint\n");
            continue;
        }

        DeviceProps device;
        if (SUCCEEDED(getDeviceProperty(pDevice, &device)))
        {
            LPWSTR id = nullptr;
            hr = pDevice->GetId(&id);
            if (SUCCEEDED(hr) && id)
            {
                wstring currentID(id);
                device.id = currentID;
                device.isActive = (currentID == activeDevID);
                CoTaskMemFree(id);
            }
            output.push_back(device);
        }

        pDevice->Release();
    }

    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return output;
}
 
 
 
bool registerNotificationCallback(EDataFlow deviceType, ERole eRole)
{
    std::vector<DeviceProps> output;

    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        IMMDeviceEnumerator *pEnumerator = NULL;
        hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, reinterpret_cast<void **>(&pEnumerator));
        if (SUCCEEDED(hr))
        {
            IMMDevice *pActive = NULL;

            pEnumerator->GetDefaultAudioEndpoint(deviceType, eRole, &pActive);
            IMMNotificationClient *pNotify = NULL;
            pEnumerator->RegisterEndpointNotificationCallback(pNotify);
        }
    }
    return 0.0;
}
 
  
 
 
 
// static
class Win32audioPlugin : public flutter::Plugin, public IMMNotificationClient
{

public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar)
    {
        auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "win32audio",
            &flutter::StandardMethodCodec::GetInstance());

        auto plugin = std::make_unique<Win32audioPlugin>(std::move(channel));
        plugin->channel_->SetMethodCallHandler(
            [plugin_pointer = plugin.get()](const auto &call, auto result)
            {
                plugin_pointer->HandleMethodCall(call, std::move(result));
            });

        registrar->AddPlugin(std::move(plugin));
    }

    Win32audioPlugin(std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel)
        : channel_(std::move(channel)) {}

    ~Win32audioPlugin() override
    {
        if (device_enumerator_)
        {
            device_enumerator_->UnregisterEndpointNotificationCallback(this);
            device_enumerator_->Release();
        }
    }

private:
    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue> &method_call,
                          std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result)
    {
        if (method_call.method_name().compare("initAudioListener") == 0)
        {
            Initialize();
            result->Success(flutter::EncodableValue(true));
        }
        else if (method_call.method_name().compare("enumAudioDevices") == 0)
        {
            const flutter::EncodableMap &args = std::get<flutter::EncodableMap>(*method_call.arguments());
            int deviceType = std::get<int>(args.at(flutter::EncodableValue("deviceType")));
            int role = std::get<int>(args.at(flutter::EncodableValue("role")));
            std::vector<DeviceProps> devices = EnumAudioDevices((EDataFlow)deviceType, (ERole)role);
            // loop through devices and add them to a map
            flutter::EncodableMap map;
            int i = 0;
            for (const auto &device : devices)
            {
                flutter::EncodableMap deviceMap;
                deviceMap[flutter::EncodableValue("id")] = flutter::EncodableValue(Encoding::WideToUtf8(device.id));
                deviceMap[flutter::EncodableValue("name")] = flutter::EncodableValue(Encoding::WideToUtf8(device.name));
                deviceMap[flutter::EncodableValue("iconInfo")] = flutter::EncodableValue(Encoding::WideToUtf8(device.iconInfo));
                deviceMap[flutter::EncodableValue("isActive")] = flutter::EncodableValue(device.isActive);
                map[i] = flutter::EncodableValue(deviceMap);
                i++;
            }
            result->Success(flutter::EncodableValue(map));
        }  
        else
        {
            // MessageBoxA(NULL, "Method not implemented", "Win32AudioPlugin", MB_ICONWARNING | MB_OK);
            result->NotImplemented();
        }
    }

    void Initialize()
    {
        CoInitialize(nullptr);
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&device_enumerator_));
        if (device_enumerator_)
        {
            device_enumerator_->RegisterEndpointNotificationCallback(this);
        }
    }

    void Dispose()
    {
        if (device_enumerator_)
        {
            device_enumerator_->UnregisterEndpointNotificationCallback(this);
            device_enumerator_->Release();
            device_enumerator_ = nullptr;
        }
    }

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceStateChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDeviceAdded(LPCWSTR device_id) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceAdded")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDeviceRemoved(LPCWSTR device_id) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDeviceRemoved")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id) override
    {
        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnDefaultDeviceChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))},
        };
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));

        return S_OK;
    }

    STDMETHODIMP OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key) override
    {

        flutter::EncodableMap message = {
            {flutter::EncodableValue("name"), flutter::EncodableValue("OnPropertyValueChanged")},
            {flutter::EncodableValue("id"), flutter::EncodableValue(Encoding::WideToUtf8(device_id))}};
        channel_->InvokeMethod("onAudioDeviceChange", std::make_unique<flutter::EncodableValue>(message));
        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient))
        {
            *ppv = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG)
    AddRef() override
    {
        return InterlockedIncrement(&ref_count_);
    }

    STDMETHODIMP_(ULONG)
    Release() override
    {
        ULONG count = InterlockedDecrement(&ref_count_);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    void NotifyFlutter(const std::string &event)
    {
        channel_->InvokeMethod("onAudioDeviceChange", nullptr);
    }

    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
    IMMDeviceEnumerator *device_enumerator_;
    ULONG ref_count_;
};
void Win32audioPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
    Win32audioPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
