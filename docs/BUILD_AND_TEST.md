# Build and test instructions

## Target build

Requirements:

- Python 3
- PlatformIO Core
- Git

From the repository root:

```powershell
$env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = "No"
python -m platformio run -e ESP32_2432S028_2USB
```

The post-build script writes an application image and merged factory image
under `firmware/<git-version>/`. Release builds must be made from the annotated
`V1.8.3-multipool.1` tag so the output directory is stable.

## Native deterministic tests

On Windows with Visual Studio C++ Build Tools installed, first build the target
once so PlatformIO installs ArduinoJson, then run:

```powershell
./tools/run_native_tests.ps1
```

The test compiles the production `PoolRegistry`, `PoolStatsParsers`, and
`PoolStatsPolicy` modules and executes them against synthetic fixtures. It
covers:

- all supported provider selections and unknown-pool fallback;
- hostname normalization, long-name truncation, and `.worker` removal;
- Public Pool and Helios JSON field mappings;
- Helios 24-hour active-worker boundary;
- missing and malformed JSON;
- strict UTC/calendar validation;
- HTTP 200, 304, 400, 404, 429, 500, 503, and timeout classification;
- last-good caching, stale/unavailable/error state, and backoff limits.

The tests do not emulate Wi-Fi, TLS, FreeRTOS scheduling, the TFT controller, or
real mining. Those items remain part of hardware validation.

## UI render mocks

With Python and Pillow installed:

```powershell
python tools/render_pool_panel.py
```

This regenerates the images in `docs/images`. See
`docs/UI_RENDER_MOCKS.md` for the method and limitations.

## Flash addresses

- Merged factory image: flash at `0x0000`.
- Application-only firmware image: flash at `0x10000` only when the compatible
  upstream bootloader and partition table are already installed.

Use the merged factory image for a clean first test on the development board.
