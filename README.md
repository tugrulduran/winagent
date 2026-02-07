# WinAgent

WinAgent is a lightweight Windows system monitoring agent built with **C++20** and **Qt 6 Widgets**. It collects real-time system snapshots and transmits them over **UDP** to a remote server or dashboard.

The application is designed to be unobtrusive, running background monitoring modules in dedicated threads to ensure a responsive UI and minimal system impact.

> **Note:** The user interface contains some text in Turkish, while the source code, comments, and documentation are in English.

---

## 🚀 Features

WinAgent includes several specialized monitoring modules:

*   **CPU Monitor**: Tracks total CPU usage using Windows Performance Data Helpers (PDH).
*   **Memory Monitor**: Reports total, used, and free RAM via `GlobalMemoryStatusEx`.
*   **Network Monitor**: Monitors per-interface data rates (KB/s in/out) using `GetIfTable2`.
*   **Audio Monitor**: 
    *   Tracks master volume and per-application audio sessions.
    *   Listens for audio device changes.
    *   Supports switching default audio output devices (via undocumented `IPolicyConfig` interfaces).
*   **Media Monitor**: 
    *   Retrieves current media metadata (Title, Artist) and playback state.
    *   Uses WinRT `GlobalSystemMediaTransportControls`.
    *   Provides fallback to window title tracking.
*   **Process Monitor**: Identifies the current foreground application (Executable name and PID).
*   **UDP Reporter**:
    *   **Telemetry**: Sends compact binary packets to a configurable server (Default: `127.0.0.1:5000`).
    *   **Command Listener**: Listens for incoming UDP commands for remote control (Default: Port `5001`).

---

## 🏗️ Project Structure

*   `main.cpp`: Entry point; initializes the Qt application and `MainWindow`.
*   `include/BaseMonitor.h`: Abstract base class for all monitoring modules, providing thread management and consistent update intervals.
*   `include/modules/` & `src/modules/`: Implementation of specific monitoring logic:
    *   `CPUMonitor`, `MemoryMonitor`, `NetworkBytes`, `AudioMonitor`, `MediaMonitor`, `ProcessMonitor`.
    *   `AudioDeviceSwitcher`: Utility for managing Windows audio endpoints.
*   `NetworkReporter`: Manages the UDP communication lifecycle (Sending snapshots & receiving commands).
*   `MainWindow`: The Qt-based GUI that displays real-time data and manages the reporter state.

---

## ⚙️ How It Works

### Multi-Threading
WinAgent utilizes a multi-threaded architecture to ensure performance:
1.  **UI Thread**: Handles rendering and user interactions.
2.  **Monitor Threads**: Each active monitor runs in its own thread, updating its internal state at fixed intervals.
3.  **Reporter Threads**:
    *   **Sender**: Periodically aggregates data from all monitors and sends a binary packet.
    *   **Listener**: Waits for incoming commands on a separate UDP port.

### Thread Safety
Each monitor protects its internal state with a `std::mutex`. When the UI or Reporter requests data, the monitor returns a **copy** of its latest snapshot while holding the lock, preventing data races.

### Network Protocol
The agent communicates using a compact binary format to minimize bandwidth. Data is packed without padding (`#pragma pack(1)`) to ensure consistency between the agent and the receiver.

---

## 🛠️ Build Requirements

*   **Operating System**: Windows 10 or 11.
*   **Compiler**: A C++20 compatible compiler (MSVC recommended).
*   **Build System**: CMake 3.28+.
*   **Framework**: Qt 6 (Widgets and Network modules).

### Building with CMake
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 📄 License
This project is licensed under the terms provided in [src/LICENCE.txt](LICENCE.txt).
