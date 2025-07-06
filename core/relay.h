#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <string>

class AudioRelay {
public:
    AudioRelay();
    ~AudioRelay();

    void initialize();
    void shutdown();

    std::vector<IMMDevice*> getAvailableDevices() const;

    static std::wstring getDeviceName(IMMDevice* device);

    bool setLoopbackDevice(IMMDevice* device);
    bool addOutputDevice(IMMDevice* device);
    bool removeOutputDevice(IMMDevice* device);
    bool setDeviceVolume(IMMDevice* device, float volume);

    void setGlobalVolume(float volume);

    void start();
    void stop();

    bool removeAllOutputDevices();

private:
    void applyEffectiveVolume(IMMDevice* device);
    void run();
    void refreshAvailableDevices();

    std::vector<IMMDevice*> availableDevices;
    std::vector<IMMDevice*> outputDevices;
    std::map<IMMDevice*, float> deviceVolumes;
    std::map<IMMDevice*, IAudioEndpointVolume*> volumeControls;

    IMMDevice* loopbackDevice = nullptr;
    IMMDeviceEnumerator* enumerator = nullptr;

    float globalVolume = 1.0f;
    std::thread relayThread;
    std::atomic<bool> running = false;
};