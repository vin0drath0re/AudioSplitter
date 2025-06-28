#include "relay.h"
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#include <iostream>
#include <algorithm>

AudioRelay::AudioRelay() {}

AudioRelay::~AudioRelay() {
    shutdown();
}

void AudioRelay::initialize() {
    CoInitialize(nullptr);
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
}

void AudioRelay::shutdown() {
    stop();

    if (loopbackDevice) loopbackDevice->Release();
    loopbackDevice = nullptr;

    for (auto dev : outputDevices) dev->Release();
    outputDevices.clear();

    for (auto& [_, dev] : deviceMap) dev->Release();
    deviceMap.clear();

    if (deviceCollection) deviceCollection->Release();
    deviceCollection = nullptr;

    if (enumerator) enumerator->Release();
    enumerator = nullptr;

    CoUninitialize();
}

std::wstring AudioRelay::getDeviceName(IMMDevice* device) {
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

void AudioRelay::showDevices() {
    if (deviceCollection) deviceCollection->Release();
    for (auto& [_, dev] : deviceMap) dev->Release();
    deviceMap.clear();

    enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
    UINT count = 0;
    deviceCollection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        deviceCollection->Item(i, &dev);
        deviceMap[i] = dev;
    }

    std::wcout << L"Available Output Devices:\n";
    for (auto& [i, dev] : deviceMap) {
        std::wcout << L"[" << i << L"]: " << getDeviceName(dev) << std::endl;
    }
}

bool AudioRelay::setLoopbackDevice(int index) {
    if (!deviceMap.count(index)) return false;
    if (loopbackDevice) loopbackDevice->Release();
    loopbackDevice = deviceMap[index];
    loopbackDevice->AddRef();
    std::wcout << L"Loopback device set: " << getDeviceName(loopbackDevice) << std::endl;
    return true;
}

bool AudioRelay::addOutputDevice(int index) {
    if (!deviceMap.count(index)) return false;
    outputDevices.push_back(deviceMap[index]);
    deviceMap[index]->AddRef();
    std::wcout << L"Output device added: " << getDeviceName(deviceMap[index]) << std::endl;
    return true;
}

bool AudioRelay::removeOutputDevice(int index) {
    if (!deviceMap.count(index)) return false;
    IMMDevice* toRemove = deviceMap[index];
    auto it = std::find(outputDevices.begin(), outputDevices.end(), toRemove);
    if (it != outputDevices.end()) {
        (*it)->Release();
        outputDevices.erase(it);
        std::wcout << L"Output device removed: " << getDeviceName(toRemove) << std::endl;
        return true;
    }
    return false;
}

void AudioRelay::start() {
    if (running) {
        std::wcout << L"Relay already running.\n";
        return;
    }

    if (!loopbackDevice || outputDevices.empty()) {
        std::wcout << L"Set loopback and at least one output device first.\n";
        return;
    }

    running = true;
    relayThread = std::thread(&AudioRelay::run, this);
    std::wcout << L"Relay started from " << getDeviceName(loopbackDevice) << L"\n";
    for (auto& dev : outputDevices) {
        std::wcout << L"-> " << getDeviceName(dev) << L"\n";
    }
}

void AudioRelay::stop() {
    if (!running) return;
    running = false;
    if (relayThread.joinable()) relayThread.join();
    std::wcout << L"Relay stopped.\n";
}

void AudioRelay::run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    IAudioClient* captureClient = nullptr;
    IAudioCaptureClient* audioCapture = nullptr;
    WAVEFORMATEX* pwfx = nullptr;

    loopbackDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&captureClient);
    captureClient->GetMixFormat(&pwfx);
    REFERENCE_TIME hnsRequestedDuration = 2000000;
    captureClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        hnsRequestedDuration, 0, pwfx, nullptr);
    captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&audioCapture);
    captureClient->Start();

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
