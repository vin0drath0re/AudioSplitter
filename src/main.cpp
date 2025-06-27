#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#include <iostream>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

std::wstring GetDeviceName(IMMDevice* device) {
    IPropertyStore* props = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) return L"Unknown";

    PROPVARIANT varName;
    PropVariantInit(&varName);
    props->GetValue(PKEY_Device_FriendlyName, &varName);

    std::wstring name = varName.pwszVal ? varName.pwszVal : L"Unknown";
    PropVariantClear(&varName);
    props->Release();
    return name;
}

int wmain() {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::wcerr << L"CoInitialize failed: " << std::hex << hr << std::endl;
        return 1;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        std::wcerr << L"CoCreateInstance failed: " << std::hex << hr << std::endl;
        CoUninitialize();
        return 1;
    }

    IMMDeviceCollection* devices = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices);
    if (FAILED(hr)) {
        std::wcerr << L"EnumAudioEndpoints failed: " << std::hex << hr << std::endl;
        enumerator->Release();
        CoUninitialize();
        return 1;
    }

    UINT count = 0;
    devices->GetCount(&count);

    std::wcout << L"Available Output Devices:\n";
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (SUCCEEDED(devices->Item(i, &device))) {
            std::wcout << i << L": " << GetDeviceName(device) << std::endl;
            device->Release();
        }
    }

    devices->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}
