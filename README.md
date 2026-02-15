# WinAgent

<p align="center">
  <strong>WinAgent is an open-source Windows host app that loads metric plugins (DLLs), serves a secure HTTPS dashboard, and streams JSON over secure WebSockets (WSS).</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-blue" />
  <img src="https://img.shields.io/badge/build-CMake-informational" />
  <img src="https://img.shields.io/badge/language-C%2B%2B20-00599C" />
  <img src="https://img.shields.io/badge/framework-Qt%206-41CD52" />
  <img src="https://img.shields.io/badge/plugins-DLL-orange" />
</p>

---

## What is WinAgent?

**WinAgent** is an **open-source**, lightweight Windows **host** process that:

- loads **metric modules as external plugins** (`.dll`)
- serves a **static dashboard over HTTPS**
- streams **structured telemetry as JSON over WSS**

This design lets you extend the agent without changing the host: ship a new plugin DLL, drop it into the `plugins/` folder, and the dashboard (or any client) can consume the new data immediately.

---

## Features

- 🧩 **Plugin system (DLL)**
    - Host loads `plugins/*.dll`
    - Optional per-plugin config: `plugins/<id>.json`
- 🌐 **HTTPS dashboard server**
    - Serves static files (default dashboard included)
    - Default port: **3003**
- 🔌 **WSS (WebSocket Secure) JSON streaming**
    - Default port: **3004**
    - Periodic “update” events (default: 1000 ms)
- 🔒 **TLS support**
    - Uses `certs/cert.pem` + `certs/key.pem` (self-signed by default)
    - Override certificate directory via CMake (`WA_CERTS_DIR`)
- ⚙️ **CMake + Qt6 build**
- 📦 Example dashboard under `dashboards/default`

---


## Open Source & Community

WinAgent is built to be **hackable, auditable, and extensible**:

- ✅ **Open by default**: the host stays small; new capabilities ship as plugins.
- 🔍 **Transparent architecture**: telemetry is plain **JSON over WSS** and easy to inspect.
- 🧩 **Composable ecosystem**: write your own modules and share them as independent DLLs.
- 🤝 **Contributions welcome**: bug reports, feature requests, new plugins, dashboard improvements, docs.

If you use WinAgent in your lab or production and something is missing, please open an issue or send a PR.
Even small improvements (docs, examples, refactors) help the project a lot.

### Contributing

1. Fork the repo and create a branch:
    - `feature/<short-name>` or `fix/<short-name>`
2. Build in **Release** (see “Build” section).
3. Keep changes focused and add/update docs when relevant.
4. Open a Pull Request with:
    - what/why, screenshots (for dashboard changes), and how to test.

### Suggested contribution ideas

- New plugins (GPU, disks, temperatures, SMART, UPS, hypervisor stats)
- More dashboards (light theme, mobile-first, Grafana-style)
- Hardening release paths (resolve assets via `applicationDirPath()`)


## Repository layout

```
WinAgent/
├─ CMakeLists.txt
├─ main.cpp
├─ include/                 # host headers + plugin ABI (BasePlugin.h)
├─ src/                     # host sources
├─ plugins/                 # plugin projects (built as DLLs)
├─ dashboards/default/      # static dashboard served over HTTPS
├─ certs/                   # default self-signed cert/key
└─ lib/                     # 3rd-party runtime (e.g. hidapi*.dll/.lib)
```

---

## Requirements

- Windows 10/11 (x64)
- **Qt 6.x** modules: Core, Widgets, Network, WebSockets, HttpServer
- **CMake ≥ 3.28**
- Visual Studio 2022 (MSVC) recommended

> You typically provide Qt via `CMAKE_PREFIX_PATH`, e.g. `C:/Qt/6.6.3/msvc2022_64`.

---

## Build

### Visual Studio (multi-config)

```bat
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

cmake --build build --config Release --target WinAgent plugins
```

Outputs (typical):

```
build/Release/
├─ WinAgent.exe
├─ plugins/                 # plugin DLLs + <id>.json configs
├─ dashboards/default/      # dashboard static files
└─ certs/                   # TLS cert/key (or overridden WA_CERTS_DIR)
```

> The project is set up to run **windeployqt** as a post-build step on Windows (so Qt runtime DLLs land next to the executable).

---

## Run

1) Launch `WinAgent.exe`
2) Start the dashboard server from the UI (often enabled by default)

### Dashboard

Default URL:

- `https://<PC-IP>:3003/`

Browser will warn about the **self-signed certificate** — expected for dev builds.

### WebSocket stream

- `wss://<PC-IP>:3004`

---

## WebSocket JSON protocol

### Push updates

The server periodically broadcasts messages like:

```json
{
  "event": "update",
  "payload": {
    "modules": {
      "cpu": { "cores": 16, "load": 9.87, "ok": true },
      "<pluginId>": {},
      "...": {}
    }
  }
}
```

- `modules` contains snapshots collected from the loaded plugins.

### Requests / commands

Clients may send commands; the host can route requests to a module:

```json
{
  "cmd": "moduleRequest",
  "payload": {
    "module": "cpu",
    "payload": { "hello": "world" }
  }
}
```

> Some handlers may be WIP depending on the module.

---

## Plugin system

### Where plugins live

At runtime, the host scans:

```
<exe_dir>/plugins/*.dll
```

Optional per-plugin config:

```
<exe_dir>/plugins/<id>.json
```

Where `<id>` is returned by the plugin’s `wa_get_info()` (`WaPluginInfo.id`).

### Built-in plugins (in this repo)

- `cpu`
- `memory`
- `network`
- `audio`
- `audiodevice`
- `media`
- `launcher`
- `process`
- `audeze`

Each plugin has its own `CMakeLists.txt`, source files, and a `config.json` template.

### Writing a new plugin

The ABI is defined in **`include/BasePlugin.h`**.

Common exports include:
- `wa_get_info`, `wa_create`, `wa_init`, `wa_start`, `wa_stop`, `wa_destroy`
- `wa_read`, `wa_request`
- optional: `wa_pause`, `wa_resume`

Look at the existing plugin folders for working examples.

> Look at the plugins/ folder for more info about the plugin development process.

---

## TLS certificates

By default, WinAgent uses the `certs/` folder shipped with the repo:

- `certs/cert.pem`
- `certs/key.pem`

To use your own certificate directory at build time:

```bat
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" ^
  -DWA_CERTS_DIR="C:/MyCerts"
```

---

## Release / Distribution (recommended flow)

### 1) Build release binaries

```bat
cmake --build build --config Release --target WinAgent plugins
```

### 2) Stage an install folder (optional but clean)

If you added install rules (for dashboards/certs/plugins), you can stage like this:

```bat
cmake --install build --config Release --prefix dist
```

### 3) Deploy Qt runtime into `dist/bin`

In **PowerShell**, use the call operator `&` (important):

```powershell
& "C:\Qt\6.x.x\msvc2022_64\bin\windeployqt.exe" `
  --release --compiler-runtime --no-translations `
  --dir "dist\bin" "dist\bin\WinAgent.exe"
```

---

## Troubleshooting

- **Certificate warning in browser**: normal for self-signed certs.
- **Firewall**: allow ports **3003** (HTTPS) and **3004** (WSS).
- **Working directory issues**: for robust release builds, prefer resolving
  `certs/` and `dashboards/` paths relative to `QCoreApplication::applicationDirPath()`.

---

## License

MIT — see `LICENCE.txt`.

---
<p align="center">
  <img src="https://img.shields.io/badge/open--source-%E2%9C%94-brightgreen"/>
  <img src="https://img.shields.io/badge/license-MIT-green"/>
  <img src="https://img.shields.io/badge/contributions-welcome-orange"/>
</p>

