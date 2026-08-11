# qtconsole

Realtime Qt6 console for displaying incoming measurements with low latency.

## Features

- Realtime visualization panes:
  - Numeric / Ratio
  - Time Series
  - Statistics
- UDP or WebSocket input receiver
- Dockable/tabbed panes
- Config persistence (auto-save)
- Non-blocking daily CSV history archive
- Config import/export from `File` menu
- Startup CLI overrides for protocol/port/measurement title

## Build

Requirements:

- CMake >= 3.21
- C++17 compiler
- Qt6 modules: `Widgets`, `Charts`, `Network`, `WebSockets`

Build:

```bash
cmake -S . -B build
cmake --build build -j
```

Run:

```bash
./build/qtconsole
```

## Sending Data To qtconsole

The receiver accepts one numeric value per message.

Supported payload formats:

- UTF-8 text number (for example: `123.45`)
- 8-byte little-endian binary `double`

### UDP example

Using `netcat`:

```bash
echo 123.45 | nc -u -w0 127.0.0.1 9000
```

Continuous test stream with included helper script:

```bash
python3 tools/sample_sender.py --mode udp --host 127.0.0.1 --port 9000 --hz 60
```

### WebSocket example

With helper script:

```bash
python3 tools/sample_sender.py --mode ws --host 127.0.0.1 --port 9000 --hz 60
```

If using `--mode ws`, install dependency first:

```bash
pip install websockets
```

## Configuration Saving

### Auto-save

Auto-saving is enabled.

- Settings are stored per window identity (application name).
- Window identity format is:

```text
qtconsole_{receiver_protocol(UDP|WS)}{port_number}_{title_of_measurement}
```

If measurement title is empty:

```text
qtconsole_{receiver_protocol(UDP|WS)}{port_number}
```

This identity is used as the settings "name" (the `QSettings` application name).

Organization is always:

```text
kshu
```

### Where settings are saved

- Windows: INI format in user AppData (`QSettings::IniFormat`, `UserScope`)
- Linux: native user config storage (`QSettings::NativeFormat`, `UserScope`)

### Manual save/load

From the `File` menu:

- `Save Config As...` exports current configuration/layout to an INI file
- `Load Config...` imports configuration/layout from an INI file

## CLI Arguments

You can override startup settings with runtime arguments:

- `--protocol <udp|ws|websocket>`
- `--port <1..65535>` or `-p <1..65535>`
- `--measurement <title>` or `-m <title>`

Examples:

```bash
./build/qtconsole --protocol udp --port 9001 --measurement test_a
./build/qtconsole --protocol ws -p 9100 -m optimizer_run
```

Behavior:

1. App loads saved settings
2. CLI arguments override loaded values
3. Receiver starts automatically using resulting mode/port

## Notes

- Time Series supports reset, pause/resume, and export:
  - Local date/time x-axis with adaptive labels
  - Data export for the visible time range:
    `timestamp_iso8601,epoch_ms,session_id,raw_value,processed_value,averaged_value`
  - Image export: PNG or PDF
- Daily history is always saved by a separate writer process:
  - Default root: the local application-data directory under `kshu/qtconsole/history`
  - Layout: `<root>/<measurement-id>/<year>/<MMdd>.csv`
  - The append interval, archive root, and in-memory sample limit are configured from
    `Config > History Settings...`
  - Writer failures are reported in the status bar without interrupting reception
- `View` menu includes pane visibility toggles and `Always on top`.
