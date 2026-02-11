# 🚀 WinAgent: Windows System Monitoring & Control Agent

WinAgent is a high-performance, lightweight Windows monitoring agent built with **C++20** and **Qt 6**. It collects real-time system metrics, manages audio/media states, and exposes this data via a **WebSocket** server. Designed for power users and dashboard enthusiasts, WinAgent provides the raw telemetry needed to build custom system monitors or remote control interfaces.

---

## ✨ Key Features

WinAgent operates with a modular architecture, where each component runs independently:

*   **🖥️ System Telemetry**:
    *   **CPU Usage**: Real-time total load tracking via Windows Performance Data Helpers (PDH).
    *   **Memory (RAM)**: Precise tracking of total and used physical memory.
    *   **Network**: Per-interface data rate monitoring (Bytes Received/Sent).
    *   **Process Tracking**: Identifies the currently active foreground window and its PID.
*   **🔊 Advanced Audio Management**:
    *   Monitor master volume and per-application audio sessions.
    *   List and switch between available audio output devices.
    *   Remote toggle for mute/unmute states.
*   **🎵 Media Integration**:
    *   Hooks into Windows **Global System Media Transport Controls (GSMTC)**.
    *   Captures live metadata: Title, Artist, and Playback Status (Playing/Paused).
    *   Supports remote media controls: Play, Pause, Stop, Skip, and Seek.
*   **🚀 Remote Launcher**:
    *   Trigger pre-defined actions or applications via remote commands.
*   **🌐 Web Dashboards**:
    *   Built-in HTTPS server to serve custom HTML/JS dashboards.
    *   Dashboards can be easily customized and are located in the `dashboards/` directory.
*   **🎧 Specialized Hardware Support**:
    *   **Audeze Headphones**: Direct HID communication to monitor battery levels for supported Audeze headsets.
*   **🛰️ WebSocket API**:
    *   Broadcasts full system state as JSON every 500ms.
    *   Accepts incoming JSON-based commands for remote interaction.

---

## 🏗️ Project Architecture

*   **Threaded Monitors**: Each monitoring module (CPU, RAM, Audio, etc.) runs in its own dedicated thread to prevent UI blocking and ensure consistent sampling rates.
*   **Thread-Safe Data Hub**: A central repository manages state snapshots using mutex protection, ensuring data integrity between monitors and the communication layer.
*   **WebSocket Server**: Built on `QtWebSockets`, providing a robust, low-latency bi-directional communication channel.
*   **HTTPS Dashboard Server**: A custom SSL-enabled TCP server that serves dashboard assets to web clients.

---

## ⚙️ Technical Stack

*   **Language**: C++20 (using modern features for safety and performance).
*   **Framework**: Qt 6.10+ (Widgets, Network, WebSockets).
*   **APIs**: Windows SDK, WinRT (for Media), PDH (for CPU), HIDAPI (for Hardware).
*   **Build System**: CMake 3.28+.

---

## 🛠️ Getting Started

### Prerequisites
*   Windows 10 or 11.
*   **MSVC 2022** (recommended) or any C++20 compatible compiler.
*   **Qt 6 SDK** (including `HttpServer` and `WebSockets` components).
*   **CMake 3.28+**.

### Build Instructions
1. Clone the repository.
2. Generate build files and compile:
   ```powershell
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```
3. The executable `WinAgent.exe` will be located in the `bin` or `Release` directory.

### Deployment
To run WinAgent on a machine without Qt installed, use the `windeployqt` utility to package the necessary dependencies.

**Important**: The `dashboards` directory must be located in the same folder as `WinAgent.exe` for the web server to function correctly. Additionally, you must provide `cert.pem` and `key.pem` files in the same directory for SSL support.

```powershell
# In your build/Release directory
windeployqt.exe WinAgent.exe
```

---

## 📄 License

This project is licensed under the terms found in [LICENCE.txt](LICENCE.txt).
