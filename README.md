# AudioSplitter

A real-time Windows desktop application that captures system audio using WASAPI and relays it to multiple output devices. Built with **C++** and a **Qt GUI**, it acts as a virtual audio splitter with an intuitive interface.


## Features

- Real-time system audio capture using WASAPI loopback
- GUI built with Qt Widgets for selecting source and output devices
- Route audio to multiple playback devices simultaneously
- Low-latency streaming using multithreading



## Build Instructions (Qt Creator)

### Prerequisites

- Qt Creator (latest version)
- Qt 6.x with MSVC or MinGW (matching your kit)
- Windows OS



### Steps to Build in Qt Creator

1. **Open Qt Creator**
2. **File → Open File or Project**
   - Select the `AudioSplitterQt.pro` file
3. **Select your kit**
   - Example: `Desktop Qt 6.5.3 MSVC 2019 64bit`
4. **Build and Run**
   - Qt Creator will handle compilation, linking, and running




## Project Structure

```
.
├── core/
│   ├── relay.cpp
│   └── relay.h
└── gui/
    ├── components/
    │   ├── DeviceItemWidget.cpp
    │   └── DeviceItemWidget.h
    ├── mainwindow.cpp
    ├── mainwindow.h
    ├── mainwindow.ui
    ├── AudioSplitterQt.pro
    └── main.cpp
```



## Author

Developed by Vinod Singh Rathore  
[https://github.com/vin0drath0re](https://github.com/vin0drath0re)

