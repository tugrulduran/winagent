# WinAgent

<p align="center">
  <strong>🚀 A Modern, Open‑Source Windows System Monitoring Agent</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-blue?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B17-00599C?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/framework-Qt%206-41CD52?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/build-CMake-informational?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/license-MIT-green?style=for-the-badge"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/open--source-%E2%9C%94-brightgreen"/>
  <img src="https://img.shields.io/badge/contributions-welcome-orange"/>
  <img src="https://img.shields.io/badge/status-active%20development-blueviolet"/>
</p>

---

## 🧠 What is WinAgent?

**WinAgent** is a **fully open‑source**, high‑performance **Windows system monitoring agent** written in modern **C++ (Qt 6)**.

It is designed to run **continuously**, collect **real‑time system, media, and hardware metrics**, and expose them through a **WebSocket‑based JSON API** that can be consumed by:

- Web dashboards
- Desktop monitoring apps
- Automation systems
- Home‑lab / observability stacks

WinAgent is **backend‑only by design** — UI, dashboards, and visualization layers are intentionally decoupled.

---

## ✨ Key Features

- 🧩 **Plugin‑like modular monitor system**
- 📊 **Real‑time metrics**
    - CPU usage
    - Memory usage
    - Network activity
    - Audio activity & devices
    - Media playback state
    - Application / launcher status
- 🎧 **Audeze Maxwell integration**
    - Battery & device state via HID
- 🌐 **WebSocket server**
    - Push‑based JSON messages
    - Timer‑driven, low‑latency updates
- 🧵 **Thread‑safe data sharing**
    - Atomics & minimal locking
- ⚙️ **CMake‑based build**
- 📖 Clean, readable, extensible C++ codebase

---

## 🏗 High‑Level Architecture

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
|   DashboardData     |
|---------------------|
| Thread-safe store   |
| Atomic metrics      |
+----------+----------+
           |
           v
+---------------------+
| WebSocket Server    |
|---------------------|
| JSON push updates   |
| Timer-based send    |
+----------+----------+
           |
           v
+---------------------+
| External Dashboards |
| Web / Desktop / CLI |
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

### System Requirements

- Windows 10 / 11 (x64)
- **Qt 6.x**
- **CMake ≥ 3.20**
- MSVC (Visual Studio 2022 recommended)

---

### 🔧 Build Instructions

```bash
git clone https://github.com/your-org/winagent.git
cd winagent

mkdir build
cd build

cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

The resulting executable will be generated under:

```
build/Release/
```

---

## ▶️ Running WinAgent

Simply run the generated executable:

```bash
WinAgent.exe
```

Once running:
- System monitors start automatically
- WebSocket server is initialized
- Metrics begin broadcasting at fixed intervals

WinAgent is designed to be **long‑running** (days / weeks uptime).

---

## 🌐 WebSocket API

WinAgent exposes a **push‑only WebSocket server**.

### Example Message

```json
{
  "cmd": "cpuUpdate",
  "payload": {
    "usage": 7.43
  }
}
```

### Design Principles

- No polling
- No shared mutable state across modules
- Central `DashboardData` store
- Clear command‑based JSON schema

---

## 🧩 Writing a New Monitor

Creating a new monitor is straightforward:

1. Inherit from `BaseMonitor`
2. Implement:
    - `start()`
    - `stop()`
    - update loop
3. Write results into `DashboardData`
4. Register the module in `ModuleFactory`

No changes are required in the WebSocket layer.

---

## 🔒 Thread Safety Model

- All shared state lives in `DashboardData`
- Uses `std::atomic` where possible
- Minimal mutex usage
- Monitors run independently
- WebSocket server reads on its own timer thread

This design minimizes contention and avoids hidden dependencies.

---

## 🎯 Project Philosophy

- ✅ Open‑source first
- ✅ Maintainable over clever
- ✅ Backend‑only, UI‑agnostic
- ✅ Designed for real, long‑running systems
- ❌ No magic, no global state chaos

---

## 🛣 Roadmap

- [ ] Authentication for WebSocket clients
- [ ] Configurable update intervals
- [ ] YAML / JSON config file
- [ ] Plugin loading (DLL‑based)
- [ ] Linux support

---

## 🤝 Contributing

Contributions are **very welcome**.

- Fork the repo
- Create a feature branch
- Keep code clean and readable
- Open a PR

Even small improvements or monitor ideas matter ❤️

---

## 📜 License

This project is licensed under the **MIT License**.

You are free to:
- Use
- Modify
- Distribute
- Embed

Just keep the license and don’t blame the author 😉

---

## ⭐ Final Words

WinAgent is built for developers who care about:
**correctness, performance, and architectural sanity**.

If that sounds like you — welcome aboard 🚀
