#define INITGUID
#include "relay.h"

#include <comdef.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>

AudioRelay::AudioRelay() {}

AudioRelay::~AudioRelay() { shutdown(); }

void AudioRelay::initialize() {
  CoInitialize(nullptr);
  CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                   IID_PPV_ARGS(&enumerator));
  refreshAvailableDevices();
}

void AudioRelay::shutdown() {
  stop();

  if (loopbackDevice) loopbackDevice->Release();
  loopbackDevice = nullptr;

  for (auto dev : outputDevices) dev->Release();
  outputDevices.clear();

  for (auto dev : availableDevices) dev->Release();
  availableDevices.clear();

  for (auto& [dev, vol] : volumeControls) {
    if (vol) vol->Release();
  }
  volumeControls.clear();
  deviceVolumes.clear();

  if (enumerator) enumerator->Release();
  enumerator = nullptr;

  CoUninitialize();
}

void AudioRelay::refreshAvailableDevices() {
  for (auto dev : availableDevices) dev->Release();
  availableDevices.clear();

  IMMDeviceCollection* collection = nullptr;
  enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  UINT count = 0;
  collection->GetCount(&count);

  for (UINT i = 0; i < count; ++i) {
    IMMDevice* dev = nullptr;
    collection->Item(i, &dev);

    // Check if the device is not in outputDevices and not the loopbackDevice
    if (std::find(outputDevices.begin(), outputDevices.end(), dev) ==
            outputDevices.end() &&
        dev != loopbackDevice) {
      availableDevices.push_back(dev);
    } 
  }
  collection->Release();
}

std::vector<IMMDevice*> AudioRelay::getAvailableDevices() const {
  return availableDevices;
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

bool AudioRelay::setLoopbackDevice(IMMDevice* device) {
  if (!device) return false;
  if (loopbackDevice) {
    availableDevices.push_back(loopbackDevice);
    loopbackDevice->Release();
  }
  availableDevices.erase(
      std::remove(availableDevices.begin(), availableDevices.end(), device),
      availableDevices.end());
  loopbackDevice = device;
  loopbackDevice->AddRef();
  return true;
}

bool AudioRelay::addOutputDevice(IMMDevice* device) {
  if (!device) return false;

  availableDevices.erase(
      std::remove(availableDevices.begin(), availableDevices.end(), device),
      availableDevices.end());
  device->AddRef();
  outputDevices.push_back(device);

  IAudioEndpointVolume* endpointVolume = nullptr;
  HRESULT hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                nullptr, (void**)&endpointVolume);
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

  return true;
}

bool AudioRelay::removeOutputDevice(IMMDevice* device) {
  if (!device) return false;

  auto it = std::find(outputDevices.begin(), outputDevices.end(), device);
  if (it != outputDevices.end()) {
    availableDevices.push_back(*it);
    (*it)->Release();
    outputDevices.erase(it);

    if (volumeControls.count(device)) {
      volumeControls[device]->Release();
      volumeControls.erase(device);
    }
    deviceVolumes.erase(device);

    return true;
  }
  return false;
}

bool AudioRelay::removeAllOutputDevices() {
  if (outputDevices.empty()) return false;

  for (auto dev : outputDevices) {
    availableDevices.push_back(dev);
    dev->Release();
    if (volumeControls.count(dev)) {
      volumeControls[dev]->Release();
    }
  }
  outputDevices.clear();
  volumeControls.clear();
  deviceVolumes.clear();
  return true;
}

void AudioRelay::applyEffectiveVolume(IMMDevice* device) {
  if (!volumeControls.count(device)) return;
  float individual = deviceVolumes.count(device) ? deviceVolumes[device] : 1.0f;
  float effective = std::clamp(globalVolume * individual, 0.0f, 1.0f);
  volumeControls[device]->SetMasterVolumeLevelScalar(effective, nullptr);
}

bool AudioRelay::setDeviceVolume(IMMDevice* device, float volume) {
  if (!device) return false;
  deviceVolumes[device] = std::clamp(volume, 0.0f, 1.0f);
  applyEffectiveVolume(device);
  return true;
}

void AudioRelay::setGlobalVolume(float volume) {
  globalVolume = std::clamp(volume, 0.0f, 1.0f);
  for (auto dev : outputDevices) {
    applyEffectiveVolume(dev);
  }
}

void AudioRelay::start() {
  if (running || !loopbackDevice || outputDevices.empty()) return;
  running = true;
  relayThread = std::thread(&AudioRelay::run, this);
}

void AudioRelay::stop() {
  if (!running) return;
  running = false;
  if (relayThread.joinable()) relayThread.join();
}

void AudioRelay::run() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

  IAudioClient* captureClient = nullptr;
  IAudioCaptureClient* audioCapture = nullptr;
  WAVEFORMATEX* pwfx = nullptr;

  loopbackDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           (void**)&captureClient);
  captureClient->GetMixFormat(&pwfx);
  REFERENCE_TIME hnsRequestedDuration = 2000000;
  captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration,
                            0, pwfx, nullptr);
  captureClient->GetService(__uuidof(IAudioCaptureClient),
                            (void**)&audioCapture);
  captureClient->Start();

  std::vector<IAudioClient*> renderClients;
  std::vector<IAudioRenderClient*> renderServices;

  for (auto device : outputDevices) {
    IAudioClient* renderClient = nullptr;
    IAudioRenderClient* renderService = nullptr;
    device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                     (void**)&renderClient);
    renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration,
                             0, pwfx, nullptr);
    renderClient->GetService(__uuidof(IAudioRenderClient),
                             (void**)&renderService);
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
