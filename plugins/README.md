# WinAgent Plugin Development Guide

This document is the **complete, developer-facing reference** for building WinAgent plugins as external **Windows DLLs**.

You should be able to implement, build, deploy, and debug a new plugin end‑to‑end with this guide.

> Scope: **WinAgent plugin ABI + BasePlugin helper** as implemented in `include/BasePlugin.h`,
> and the runtime loader behavior implemented in `src/PluginManager.cpp`.

---

## 1) Mental model: what a WinAgent plugin is

A WinAgent plugin is a **dynamically loaded library** (`.dll`) that:

1. Exposes a small **C ABI** (stable function names and calling convention).
2. Produces a **JSON object snapshot** (as UTF‑8) when the host calls `wa_read()`.
3. Optionally handles **JSON requests/commands** (as UTF‑8) when the host calls `wa_request()`.

At runtime, the host:

- scans `<exe_dir>/plugins/*.dll`
- loads each DLL
- resolves required exports (by exact symbol name)
- calls the lifecycle methods (`wa_create → wa_init → wa_start`)
- periodically reads plugin snapshots and broadcasts them upstream.

---

## 2) Runtime discovery & config files

### 2.1 Plugin directory (runtime)

At runtime, the host loads plugins from:

```
<exe_dir>/plugins/
  ├─ *.dll
  └─ <id>.json           (optional per-plugin config)
```

### 2.2 How the host chooses the config file name

The host asks your DLL for `WaPluginInfo` (via `wa_get_info()`).
It then loads a config file named exactly:

```
<exe_dir>/plugins/<WaPluginInfo.id>.json
```

If the file does not exist, the host passes `nullptr` as `configJsonUtf8` to `wa_create()`.

**Important:** Your `WaPluginInfo.id` must be a stable, unique identifier. Changing it changes:
- your module key in aggregated JSON
- your config file name
- how the host routes requests to you (by id)

---

## 3) ABI reference (Host ↔ Plugin boundary)

The ABI is defined in `include/BasePlugin.h`.

### 3.1 API version

```
static constexpr uint32_t WA_PLUGIN_API_VERSION = 1;
```

Your plugin must set `WaPluginInfo.apiVersion` to `WA_PLUGIN_API_VERSION`.
The loader rejects mismatched versions.

### 3.2 Types

#### 3.2.1 `WaView`

A `WaView` is a *non-owning* view of a UTF‑8 byte buffer:

```cpp
struct WaView {
    const char* ptr;
    uint32_t    len;
};
```

The buffer is owned by the plugin. The host will parse/copy it immediately.

**Lifetime rule (required):**
- `wa_read()` must return a buffer that stays valid **until the next `wa_read()` call** for the same plugin handle.
- `wa_request()` must return a buffer that stays valid **until the next `wa_request()` call** for the same plugin handle.

The provided `BasePlugin` implementation already guarantees this.

#### 3.2.2 `WaPluginInfo`

```cpp
struct WaPluginInfo {
    uint32_t    apiVersion;        // must equal WA_PLUGIN_API_VERSION
    const char* id;                // stable unique id (e.g., "cpu")
    const char* name;              // display name
    uint32_t    defaultIntervalMs; // plugin default sampling interval
};
```

#### 3.2.3 Return codes (`WaRc`)

```cpp
enum WaRc : int32_t {
    WA_OK = 0,
    WA_ERR = 1,
    WA_ERR_BAD_STATE = 2,
    WA_ERR_BAD_ARG = 3,
};
```

### 3.3 Calling convention / exports

On Windows:

- all ABI functions use `__cdecl`
- exports must have *exact* names (no C++ name mangling)
- when building a plugin DLL, compile with `WA_BUILDING_PLUGIN` so exports get `__declspec(dllexport)`

### 3.4 Required exports

Your DLL must export these symbols:

```cpp
WA_EXPORT const WaPluginInfo* WA_CALL wa_get_info();

WA_EXPORT void*   WA_CALL wa_create(void* hostCtx, const char* configJsonUtf8);
WA_EXPORT int32_t WA_CALL wa_init(void* handle);
WA_EXPORT int32_t WA_CALL wa_start(void* handle);
WA_EXPORT int32_t WA_CALL wa_stop(void* handle);
WA_EXPORT void    WA_CALL wa_destroy(void* handle);

WA_EXPORT WaView  WA_CALL wa_request(void* handle, const char* requestJsonUtf8);
WA_EXPORT WaView  WA_CALL wa_read(void* handle);
```

### 3.5 Optional exports

These are optional today (the loader resolves them, but does not require them):

```cpp
WA_EXPORT int32_t WA_CALL wa_pause(void* handle);   // optional; may be no-op
WA_EXPORT int32_t WA_CALL wa_resume(void* handle);  // optional; may be no-op
```

### 3.6 The `hostCtx` parameter

`wa_create(void* hostCtx, ...)` receives an opaque pointer from the host.
In the current codebase it is **not used by the host**, but you can treat it as:
- reserved for future host services / callbacks
- a user data context if you own both host + plugin

Do not assume its type unless you control the host.

---

## 4) The recommended implementation: `BasePlugin`

Most WinAgent plugins in this repo are written by subclassing `BasePlugin`.

### 4.1 What `BasePlugin` does for you

`BasePlugin` provides:

- a sampling **worker thread**
- built-in **state machine** (constructed → inited → running/paused → stopped)
- parsing of `configJsonUtf8` into a `QJsonObject`
- snapshot serialization to **compact UTF‑8 JSON**
- stable `WaView` buffers for `wa_read()` and `wa_request()`
- an overridable `onRequest()` handler

### 4.2 Lifecycle in `BasePlugin`

The core lifecycle methods:

- `init()` → calls `onInit(QString& err)`
- `start()` → starts/resumes the worker thread and begins periodic ticks
- `pause()` / `resume()` → switches running state (worker waits while paused)
- `stop()` → requests the worker thread to stop and joins it, then calls `onStop()`

### 4.3 Sampling / `onTick()`

You must implement:

```cpp
virtual QJsonObject onTick() = 0;
```

`onTick()` is called periodically on the worker thread while the plugin is running.

After each tick, `BasePlugin` serializes the returned `QJsonObject` with:

```cpp
QJsonDocument(obj).toJson(QJsonDocument::Compact)
```

and stores it as the latest snapshot.

### 4.4 Requests / `onRequest()`

`BasePlugin::requestView()` parses `requestJsonUtf8` as a JSON object.
- If parsing fails: it returns `{ "ok": false, "error": "bad_json", "details": "<qt error>" }`
- If parsing succeeds: it calls your `onRequest(const QJsonObject&)`

Default implementation (if you don’t override): `{ "ok": true }`.

**Your request handler is synchronous.** Keep it fast; avoid blocking for long operations.

### 4.5 Config handling

When you construct `BasePlugin(defaultIntervalMs, configJsonUtf8)`, it:

- parses the config JSON (if provided) into `config()`
- if `config.intervalMs` exists and is > 0, it overrides the interval for you

You can implement additional config keys freely.

### 4.6 Default snapshot behavior

Before the plugin starts producing real data, `BasePlugin` initializes the snapshot as:

```json
{ "ok": false, "error": "not_started" }
```

---

## 5) Threading & concurrency rules

### 5.1 What can run concurrently?

- `onTick()` runs on the plugin’s worker thread.
- `onRequest()` runs on the host thread that calls `wa_request()`.
- `wa_read()` can be called frequently by the host as it aggregates snapshots.

Therefore, **you must assume `onTick()` and `onRequest()` may run at the same time.**

### 5.2 What `BasePlugin` protects (and what it does not)

`BasePlugin` protects:
- internal snapshot buffers (`latest_`, `readBuf_`, `replyBuf_`) with a mutex
- state changes with atomics/condition_variable

`BasePlugin` does **not** protect:
- your plugin’s metric state, caches, device handles, etc.

If you share state between `onTick()` and `onRequest()`, you must add your own synchronization:
- `std::mutex`, `std::shared_mutex`, atomics, lock-free queue, etc.

### 5.3 Keep `onTick()` quick

Because `onTick()` runs periodically, it should:
- avoid long blocking I/O
- avoid large allocations on every tick
- prefer incremental/rolling metrics
- move slow work to background threads if needed

---

## 6) JSON contracts and best practices

### 6.1 Snapshot must be a JSON object

The host parses snapshot bytes as a **JSON object**.
If parsing fails or the root is not an object, the host treats it as `{}` and may omit it.

### 6.2 Recommended snapshot structure

The host does not enforce a schema, but for compatibility and debuggability, follow these conventions:

- `ok` (bool): overall health of the plugin tick
- `error` (string): error code when `ok=false`
- `id` (string): your plugin id (optional but helpful)
- `name` (string): display name (optional but helpful)
- metrics fields: numbers/strings/objects you expose
- `meta` (object): non-metric debug info, versions, configuration, etc.
- `log` (object/array): lightweight stats or recent events if you implement logging

**Example:**

```json
{
  "ok": true,
  "id": "myplugin",
  "name": "My Plugin",
  "intervalMs": 1000,
  "metrics": {
    "value": 42,
    "rate": 1.5
  },
  "meta": {
    "driver": "1.2.3",
    "deviceCount": 2
  }
}
```

### 6.3 Requests: choose a convention and document it

The host routes a request payload (JSON object) to your plugin.
Inside the plugin, adopt a stable pattern so dashboards/clients can interact with you.

A common and recommended convention is:

```json
{ "cmd": "<commandName>", ...additional fields... }
```

Return:

- `ok` (bool)
- `error` (string) if `ok=false`
- optional `details` (string/object) for debugging

This is what the example `dummy` plugin does.

---

## 7) Minimal plugin template (copy/paste)

### 7.1 The plugin class (C++ / BasePlugin)

```cpp
#include "BasePlugin.h"
#include <QJsonObject>
#include <QString>

static WaPluginInfo INFO{
    WA_PLUGIN_API_VERSION,
    "myplugin",            // <--- unique stable id
    "My Plugin",           // display name
    1000                   // default interval
};

class MyPlugin final : public BasePlugin {
public:
    explicit MyPlugin(const char* configJsonUtf8)
        : BasePlugin(INFO.defaultIntervalMs, configJsonUtf8) {}

protected:
    bool onInit(QString& err) override {
        Q_UNUSED(err);
        // Read configuration:
        // const QJsonObject& c = config();
        // ...
        return true;
    }

    QJsonObject onTick() override {
        // Produce a JSON object snapshot
        return QJsonObject{
            {"ok", true},
            {"id", QString::fromUtf8(INFO.id)},
            {"value", 123}
        };
    }

    QJsonObject onRequest(const QJsonObject& req) override {
        const QString cmd = req.value("cmd").toString();
        if (cmd == "ping") return QJsonObject{{"ok", true}, {"pong", true}};
        return QJsonObject{{"ok", false}, {"error", "unknown_cmd"}, {"cmd", cmd}};
    }
};

// ---- C ABI exports ----
WA_EXPORT const WaPluginInfo* WA_CALL wa_get_info() { return &INFO; }

WA_EXPORT void* WA_CALL wa_create(void*, const char* cfg) {
    return new MyPlugin(cfg);
}

WA_EXPORT int32_t WA_CALL wa_init(void* h) {
    return h ? ((MyPlugin*)h)->init() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_start(void* h) {
    return h ? ((MyPlugin*)h)->start() : WA_ERR_BAD_ARG;
}

WA_EXPORT int32_t WA_CALL wa_stop(void* h) {
    return h ? ((MyPlugin*)h)->stop() : WA_ERR_BAD_ARG;
}

WA_EXPORT void WA_CALL wa_destroy(void* h) {
    if (!h) return;
    auto* p = (MyPlugin*)h;
    p->stop();
    delete p;
}

WA_EXPORT WaView WA_CALL wa_request(void* h, const char* reqJsonUtf8) {
    return h ? ((MyPlugin*)h)->requestView(reqJsonUtf8) : WaView{nullptr, 0};
}

WA_EXPORT WaView WA_CALL wa_read(void* h) {
    return h ? ((MyPlugin*)h)->readView() : WaView{nullptr, 0};
}
```

### 7.2 Optional: pause/resume exports

If you want to expose pause/resume via ABI:

```cpp
WA_EXPORT int32_t WA_CALL wa_pause(void* h)  { return h ? ((MyPlugin*)h)->pause()  : WA_ERR_BAD_ARG; }
WA_EXPORT int32_t WA_CALL wa_resume(void* h) { return h ? ((MyPlugin*)h)->resume() : WA_ERR_BAD_ARG; }
```

---

## 8) Building plugins in this repo (CMake)

The repo is structured so each plugin has its own subdirectory under:

```
plugins/<pluginName>/
  ├─ CMakeLists.txt
  ├─ Plugin.cpp
  ├─ config.json          (template / default config)
  └─ src/...
```

`plugins/CMakeLists.txt` auto-discovers subdirectories and registers plugin targets into an aggregated `plugins` build target.

### 8.1 Typical plugin CMakeLists.txt (pattern)

Here is the key pattern used by the `dummy` plugin:

- Build a shared library (DLL)
- Define `WA_BUILDING_PLUGIN` so ABI exports are exported
- Include the shared plugin ABI headers (`include/`)
- Link against `Qt6::Core` (if using `BasePlugin` / QJson)
- Copy `config.json` to `<exe_dir>/plugins/<id>.json` after build
- Register the target via `wa_register_plugin_target(...)`

**Notes:**
- The *output file name* of the DLL is not important to the loader; it scans `*.dll`.
- The *config file name* **must** match your `WaPluginInfo.id`.

A robust CMake skeleton:

```cmake
set(PLUGIN_ID "myplugin")
set(tgt WinAgentPlugin_${PLUGIN_ID})

add_library(${tgt} SHARED
    ${PROJECT_SOURCE_DIR}/src/BasePlugin.cpp
    Plugin.cpp
    # add your sources here...
)

target_compile_definitions(${tgt} PRIVATE WA_BUILDING_PLUGIN)
target_include_directories(${tgt} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${tgt} PRIVATE Qt6::Core)

if (TARGET WinAgent)
    set_target_properties(${tgt} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins"
        LIBRARY_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:WinAgent>/plugins"
    )
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WinAgent>/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:WinAgent>/plugins/${PLUGIN_ID}.json"
    )
endif()

wa_register_plugin_target(${tgt})

install(TARGETS ${tgt}
    RUNTIME DESTINATION bin/plugins
    LIBRARY DESTINATION bin/plugins
)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
    DESTINATION bin/plugins
    RENAME "${PLUGIN_ID}.json"
)
```

### 8.2 Building just plugins

From the repo root:

```bat
cmake --build build --config Debug --target plugins
```

or build both host + plugins:

```bat
cmake --build build --config Release --target WinAgent plugins
```

---

## 9) Deploying a plugin

To deploy to an existing WinAgent installation, you need:

1. `YourPlugin.dll` (any filename, must export the ABI correctly)
2. `<id>.json` (optional config, name must match `WaPluginInfo.id`)

Copy both to:

```
<WinAgentInstall>/plugins/
```

Restart WinAgent (or reload if you later add hot-reload).

---

## 10) Common pitfalls & troubleshooting

### 10.1 Plugin does not load at all

Checklist:

- DLL is in `<exe_dir>/plugins/`
- DLL exports all **required** symbols:
    - `wa_get_info`, `wa_create`, `wa_init`, `wa_start`, `wa_stop`, `wa_destroy`, `wa_read`, `wa_request`
- `wa_get_info()->apiVersion == WA_PLUGIN_API_VERSION`
- `wa_get_info()->id` is non-null and stable

If missing required exports, the loader logs: `Missing exports`.

### 10.2 Plugin loads but snapshot is missing/empty

- Ensure `wa_read()` returns valid **JSON object** bytes (`{...}`).
- If using `BasePlugin`, ensure your `onTick()` returns a non-empty object.
- Avoid returning arrays as root; the host expects objects.

### 10.3 Config is not applied

- Config file must be named `<id>.json`, where `<id>` is your `WaPluginInfo.id`.
- Config root must be a JSON object (not array/string/etc).
- If you want to override sampling interval, use:

```json
{ "intervalMs": 500 }
```

### 10.4 Race conditions / crashes on request

Because `onRequest()` can run concurrently with `onTick()`, protect shared state.

If you mutate metrics/state in both, add a mutex:

```cpp
std::mutex mu_;
```

and guard access in both methods.

### 10.5 Long stalls

If `onTick()` blocks (device I/O, network, disk), your snapshot rate will stall.
Move slow work to:
- a separate thread
- cached reads
- non-blocking APIs

---

## 11) Extension ideas (recommended patterns)

- Expose a `get_state` command returning current metrics and configuration.
- Expose `set_interval` to allow dashboards to tune sampling.
- Keep snapshots small; provide heavy debug data (like log dumps) only on request.
- Add `meta.version` and `meta.build` fields for diagnostics.

---

## 12) Reference: the `dummy` example

The `dummy` plugin demonstrates:

- reading config with defaults
- periodic snapshots with rolling min/max
- a small ring buffer log
- request commands (`ping`, `get_state`, `set_interval`, `pause`, `resume`, `log_dump`, ...)

Use it as a template by copying the folder and changing:
- `WaPluginInfo INFO` (`id`, `name`, `defaultIntervalMs`)
- CMake target naming and `PLUGIN_ID`
- config keys
- `onTick()` / `onRequest()` logic

---

## Appendix A: Implementing the ABI without Qt / BasePlugin

You can implement the ABI in plain C/C++ without Qt.

Requirements:

- return UTF‑8 JSON object bytes from `wa_read()`
- return UTF‑8 JSON object bytes from `wa_request()`
- maintain buffer lifetime until next call of the same function per handle

A common approach:
- store `std::string latestSnapshot; std::string lastReply;`
- return `{latestSnapshot.data(), (uint32_t)latestSnapshot.size()}`

If you do this, you can use any JSON library (or manual formatting), as long as the output is valid JSON.

---

## Appendix B: Compatibility & versioning strategy

- **Do not change `WaPluginInfo.id`** once published.
- Use `WA_PLUGIN_API_VERSION` to gate breaking ABI changes.
- For non-breaking improvements, evolve your JSON schema:
    - add new fields (safe)
    - keep old fields when possible
    - document changes for dashboard clients

---

**Happy hacking.** Drop your DLL into `plugins/` and WinAgent will pick it up.
