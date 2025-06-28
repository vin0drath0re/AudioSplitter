#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

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

struct AudioRelay {
    IMMDevice* loopbackDevice;
    std::vector<IMMDevice*> outputDevices;
    std::atomic<bool> running;

    AudioRelay(IMMDevice* loopback, std::vector<IMMDevice*> outputs)
        : loopbackDevice(loopback), outputDevices(outputs), running(true) {
    }

    void Run() {
        // Boost thread priority for low-latency audio
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        IAudioClient* captureClient = nullptr;
        IAudioCaptureClient* audioCapture = nullptr;

        WAVEFORMATEX* pwfx = nullptr;
        loopbackDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&captureClient);
        captureClient->GetMixFormat(&pwfx);
        REFERENCE_TIME hnsRequestedDuration = 2000000; // 2ms
        captureClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK,
            hnsRequestedDuration, 0, pwfx, nullptr);

        captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&audioCapture);
        captureClient->Start();

        std::vector<std::thread> renderThreads;
        std::vector<IAudioClient*> renderClients;
        std::vector<IAudioRenderClient*> renderServices;

        for (auto device : outputDevices) {
            IAudioClient* renderClient = nullptr;
            IAudioRenderClient* renderService = nullptr;
            device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&renderClient);
            renderClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                0,
                hnsRequestedDuration, 0, pwfx, nullptr);

            renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&renderService);
            renderClient->Start();
            renderClients.push_back(renderClient);
            renderServices.push_back(renderService);
        }

        while (running) {
            UINT32 packetLength = 0;
            audioCapture->GetNextPacketSize(&packetLength);
            if (packetLength == 0) {
                Sleep(1);
                continue;
            }

            BYTE* data;
            UINT32 numFrames;
            DWORD flags;
            audioCapture->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);

            for (size_t i = 0; i < renderClients.size(); ++i) {
                UINT32 padding;
                renderClients[i]->GetCurrentPadding(&padding);
                UINT32 availableFrames;
                renderClients[i]->GetBufferSize(&availableFrames);
                availableFrames -= padding;

                if (availableFrames >= numFrames) {
                    BYTE* renderData;
                    renderServices[i]->GetBuffer(numFrames, &renderData);
                    memcpy(renderData, data, numFrames * pwfx->nBlockAlign);
                    renderServices[i]->ReleaseBuffer(numFrames, 0);
                }
            }

            audioCapture->ReleaseBuffer(numFrames);
        }

        captureClient->Stop();
        captureClient->Release();
        audioCapture->Release();
    }
};

int wmain(int argc, wchar_t* argv[]) {
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

    if (argc < 3) {
        std::wcout << L"Usage: AudioSplitter.exe <source index> <target index 1> [target index 2] ..." << std::endl;
        return 0;
    }

    int sourceIndex = _wtoi(argv[1]);
    IMMDevice* sourceDevice = nullptr;
    devices->Item(sourceIndex, &sourceDevice);

    std::vector<IMMDevice*> targets;
    for (int i = 2; i < argc; ++i) {
        int targetIndex = _wtoi(argv[i]);
        IMMDevice* dev = nullptr;
        devices->Item(targetIndex, &dev);
        targets.push_back(dev);
    }

    std::wcout << L"Relaying audio from: " << GetDeviceName(sourceDevice) << std::endl;
    for (auto t : targets)
        std::wcout << L" -> " << GetDeviceName(t) << std::endl;
    std::wcout << L"Press Ctrl+C to stop..." << std::endl;

    AudioRelay relay(sourceDevice, targets);
    relay.Run();

    devices->Release();
    enumerator->Release();
    CoUninitialize();
    return 0;
}
