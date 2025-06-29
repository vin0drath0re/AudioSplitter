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

    for (auto& [dev, volume] : volumeControls) {
        if (volume) volume->Release();
    }
    volumeControls.clear();

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

    
    IMMDevice* device = deviceMap[index];
    device->AddRef();
    outputDevices.push_back(device);

    IAudioEndpointVolume* endpointVolume = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&endpointVolume);
    if (SUCCEEDED(hr)) {
        volumeControls[device] = endpointVolume;

         float currentVolume = 1.0f;
        if (SUCCEEDED(endpointVolume->GetMasterVolumeLevelScalar(&currentVolume))) {
            deviceVolumes[device] = currentVolume / globalVolume; 
        } else {
            deviceVolumes[device] = 1.0f; 
        }

        applyEffectiveVolume(device);

    }

    
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
        if (volumeControls.count(toRemove)) {
        volumeControls[toRemove]->Release();
        volumeControls.erase(toRemove);
        }
        
        if (deviceVolumes.count(toRemove)) {
            deviceVolumes.erase(toRemove);
        }
        std::wcout << L"Output device removed: " << getDeviceName(toRemove) << std::endl;
        return true;
    }


    return false;
}

bool AudioRelay::removeAllOutputDevices() {
    if (outputDevices.empty()) return false;

    for (auto& dev : outputDevices) {
        dev->Release();
        if (volumeControls.count(dev)) {
            volumeControls[dev]->Release();
            volumeControls.erase(dev);
        }
        if (deviceVolumes.count(dev)) {
            deviceVolumes.erase(dev);
        }
    }
    outputDevices.clear();
    std::wcout << L"All output devices removed.\n";
    return true;
}


void AudioRelay::applyEffectiveVolume(IMMDevice* device) {
    if (!volumeControls.count(device)) return;
    float individual = deviceVolumes.count(device) ? deviceVolumes[device] : 1.0f;
    float effective = std::clamp(globalVolume * individual, 0.0f, 1.0f);
    volumeControls[device]->SetMasterVolumeLevelScalar(effective, nullptr);
}


bool AudioRelay::setDeviceVolume(int index, float volume) {
    if (!deviceMap.count(index)) return false;
    IMMDevice* device = deviceMap[index];
    deviceVolumes[device] = std::clamp(volume, 0.0f, 1.0f);
    std::wcout << L"Device [" << getDeviceName(device) << L"] volume set to "
               << int(volume * 100) << L"%\n";

    applyEffectiveVolume(device);
    return true;
}


void AudioRelay::setGlobalVolume(float volume) {
    globalVolume = std::clamp(volume, 0.0f, 1.0f);
    std::wcout << L"Global volume set to " << int(globalVolume * 100) << L"%\n";

    for (auto& dev : outputDevices) {
        applyEffectiveVolume(dev);
    }
}

void AudioRelay::showStatus() {
    std::wcout << L"\n=== Audio Relay Status ===\n";

    // Global volume
    std::wcout << L"Global Volume: " << int(globalVolume * 100) << L"%\n";

    // Loopback device
    if (loopbackDevice) {
        std::wcout << L"Loopback Device: " << getDeviceName(loopbackDevice) << std::endl;
    } else {
        std::wcout << L"Loopback Device: (not set)\n";
    }

    // Output devices
    if (outputDevices.empty()) {
        std::wcout << L"No output devices.\n";
    } else {
        std::wcout << L"Output Devices:\n";
        for (IMMDevice* dev : outputDevices) {
            std::wstring name = getDeviceName(dev);

            float individual = deviceVolumes.count(dev) ? deviceVolumes[dev] : 1.0f;
            float effective = std::clamp(globalVolume * individual, 0.0f, 1.0f);

            std::wcout << L" - " << name
                       << L"\n     Individual Volume: " << int(individual * 100) << L"%"
                       << L"\n     Effective Volume:  " << int(effective * 100) << L"%\n";
        }
    }

    std::wcout << L"Relay Status: " << (running ? L"Running" : L"Stopped") << L"\n";
    std::wcout << L"===========================\n\n";
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



