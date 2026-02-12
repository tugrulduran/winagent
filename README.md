# WinAgent

**WinAgent** is a modular, high‑performance **Windows system monitoring agent** written in modern **C++ (Qt)**.  
It collects real‑time system, media, and hardware metrics and exposes them through a **WebSocket‑based dashboard API**, designed to be consumed by external UIs, dashboards, or automation tools.

This project focuses on **clean architecture, thread safety, and extensibility** — new monitors can be added with minimal friction.

---

## ✨ Features

- 🧠 **Modular monitor architecture**
- 📊 **Real‑time system metrics**
    - CPU usage
    - Memory usage
    - Network activity
    - Audio devices & audio activity
    - Media playback status
    - Application / launcher state
- 🎧 **Audeze Maxwell monitoring**
    - Battery & device status via HID
- 🌐 **WebSocket server**
    - Push‑based JSON updates
    - Low‑latency, timer‑driven broadcasts
- 🧵 **Thread‑safe shared data model**
- 🪟 **Native Windows application**
- ⚙️ **CMake‑based build system**

---

## 🏗 Architecture Overview

```
+---------------------+
|   System Monitors   |
|---------------------|
| CPU / Memory        |
| Network             |
| Audio / Media       |
| Audeze HID          |
+----------+----------+
           |
           v
+---------------------+
|  DashboardData      |
|---------------------|
| Thread‑safe store   |
| Atomic metrics      |
+----------+----------+
           |
           v
+---------------------+
| WebSocket Server    |
|---------------------|
| JSON push updates   |
| Timer‑based send    |
+---------------------+
           |
           v
+---------------------+
| External Dashboard  |
| (Web / Desktop UI)  |
+---------------------+
```

---

## 📁 Project Structure

```
WinAgent/
├── CMakeLists.txt
├── main.cpp
├── src/
│   ├── MainWindow.cpp
│   ├── DashboardServer.cpp
│   ├── DashboardWebSocketServer.cpp
│   └── modules/
│       ├── CPUMonitor.cpp
│       ├── MemoryMonitor.cpp
│       ├── NetworkMonitor.cpp
│       ├── AudioMonitor.cpp
│       ├── AudioDeviceMonitor.cpp
│       ├── MediaMonitor.cpp
│       ├── LauncherMonitor.cpp
│       └── AudezeMonitor.cpp
├── include/
│   ├── DashboardData.h
│   ├── DashboardServer.h
│   ├── DashboardWebSocketServer.h
│   ├── BaseMonitor.h
│   ├── ModuleFactory.h
│   └── modules/
└── .idea/
```

---

## 🚀 Getting Started

### Prerequisites

- Windows 10 / 11
- **Qt 6.x**
- **CMake ≥ 3.20**
- MSVC (Visual Studio 2022 recommended)
- HIDAPI (included for Windows)

---

### Build Instructions

```bash
git clone https://github.com/your-org/winagent.git
cd winagent

mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## 🌐 WebSocket API

WinAgent exposes a WebSocket server that periodically broadcasts JSON messages.

### Example Payload

```json
{
  "cmd": "cpuUpdate",
  "payload": {
    "usage": 7.43
  }
}
```

### Design Notes

- Push‑only (no polling)
- Centralized `DashboardData` store
- All monitors write, WebSocket server reads
- Lock‑free where possible (atomics)

---

## 🧩 Adding a New Monitor

1. Create a new class inheriting from `BaseMonitor`
2. Implement:
    - `start()`
    - `stop()`
    - data update logic
3. Register it in `ModuleFactory`
4. Write to `DashboardData`

That’s it — the data will automatically flow to the dashboard 🚀

---

## 🔒 Thread Safety

- Shared state lives in `DashboardData`
- Uses `std::atomic` and fine‑grained locking
- Monitors run independently
- WebSocket server reads on a timer thread

---

## 🎯 Project Goals

- **Low overhead**
- **Long‑running stability**
- **Clean C++ / Qt design**
- **Dashboard‑agnostic backend**
- **Easy extensibility**

---

## 🛣 Roadmap

- [ ] Authentication for WebSocket clients
- [ ] Configurable update intervals
- [ ] Plugin system
- [ ] Cross‑platform support (Linux)

---

## 📜 License

MIT License — do whatever you want, just don’t blame us 😉

---

## 💬 Notes

This project is actively developed and designed for **serious, long‑running system agents**, not toy dashboards.  
If you care about **correctness, performance, and maintainability**, you’re in the right place.

Happy hacking ❤️
