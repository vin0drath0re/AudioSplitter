#include "relay.h"
#include <iostream>
#include <sstream>

int wmain() {
    AudioRelay relay;
    relay.initialize();

    std::wstring line;
    std::wcout << L"\nCommands:\n"
                   << L"  show                     - List audio output devices\n"
                   << L"  set <index>             - Set loopback (source) device\n"
                   << L"  add <index>             - Add output (target) device\n"
                   << L"  remove <index>          - Remove output device\n"
                   << L"  start                   - Start audio relay\n"
                   << L"  stop                    - Stop audio relay\n"
                   << L"  exit                    - Exit program\n";
    while (true) {
        std::wcout << L"AudioSplitter> ";

        std::getline(std::wcin, line);
        std::wistringstream iss(line);
        std::wstring command;
        iss >> command;

        if (command == L"show") {
            relay.showDevices();
        } else if (command == L"set") {
            int index;
            if (iss >> index) {
                if (!relay.setLoopbackDevice(index))
                    std::wcout << L"Invalid index.\n";
            }
        } else if (command == L"add") {
            int index;
            if (iss >> index) {
                if (!relay.addOutputDevice(index))
                    std::wcout << L"Invalid index.\n";
            }
        } else if (command == L"remove") {
            int index;
            if (iss >> index) {
                if (!relay.removeOutputDevice(index))
                    std::wcout << L"Output device removed.\n";
            }
        } else if (command == L"start") {
            relay.start();
        } else if (command == L"stop") {
            relay.stop();
        } else if (command == L"exit") {
            break;
        } else {
            std::wcout << L"Unknown command.\n";
        }
    }

    relay.shutdown();
    return 0;
}
