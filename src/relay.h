#pragma once

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <memory>

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
};
