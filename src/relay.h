#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <memory>
#include <endpointvolume.h>


class AudioRelay {
public:
    AudioRelay();
    ~AudioRelay();

    void initialize();
    void shutdown();

    void showDevices();
    bool setLoopbackDevice(int index);
    bool addOutputDevice(int index);
    bool removeOutputDevice(int index);
    bool removeAllOutputDevices();
    bool setDeviceVolume(int index, float volume);
    void setGlobalVolume(float volume);

    void showStatus();
    void start();
    void stop();

private:
    std::wstring getDeviceName(IMMDevice* device);
    void run();

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* deviceCollection = nullptr;
    std::map<int, IMMDevice*> deviceMap;

    IMMDevice* loopbackDevice = nullptr;
    std::vector<IMMDevice*> outputDevices;

    std::atomic<bool> running = false;
    std::thread relayThread;

    void applyEffectiveVolume(IMMDevice* device);
    float globalVolume = 1.0f;
    std::map<IMMDevice*, float> deviceVolumes; 
    std::map<IMMDevice*, IAudioEndpointVolume*> volumeControls; 

};
