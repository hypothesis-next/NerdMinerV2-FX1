# Validation record

Validation date: 2026-09-10

## Build results

| Build | Static RAM | Application flash | Result |
| --- | ---: | ---: | --- |
| Untouched V1.8.3 `ESP32_2432S028_2USB` baseline | 55,040 bytes | 2,149,389 bytes | Pass |
| V1.8.3-multipool.1 `ESP32_2432S028_2USB` | 55,628 bytes | 2,138,841 bytes | Pass |
| V1.8.3-multipool.1 `NerdminerV2-T-HMI` regression build | 57,344 bytes | 2,198,265 bytes | Pass |
| V1.8.3-multipool.1 `wt32-sc01` regression build | 73,316 bytes | 2,588,209 bytes | Pass |

For the primary target, the static RAM delta is +588 bytes and the reported
application flash delta is -10,548 bytes. The flash reduction is mainly due to
removing the target's old 320x70 branded pool-panel image from the linked
firmware. The statistics task reserves a 10,240-byte runtime stack only when a
recognized remote provider is active; this is dynamic heap use and is not part
of the static RAM figure.

The compiler emitted existing board/framework warnings (TFT touch selection,
font/build macro redefinitions, SD-mode notices, and an Arduino framework UART
warning). No warning was emitted from the new pool-statistics source files in
the primary target build.

## Automated tests

`tools/run_native_tests.ps1` passed. It executes production registry, parser,
and policy code against the sanitized fixtures under `test/fixtures`.

Validated cases include provider selection, hostname normalization, unknown
port-2018 fallback prevention, long display names, worker suffix removal,
Public Pool parsing, Helios `bestEver`/`hashrate5m` parsing, the 24-hour active
boundary, missing fields, malformed JSON, invalid timestamps, HTTP
200/304/400/404/429/500/503, timeout classification, cache preservation, stale
state, unavailable state, and exponential backoff.

## Packaging checks

The post-build packager resolves `boot_app0.bin` through PlatformIO's installed
`framework-arduinoespressif32` package when it is not copied into the build
directory. The merged classic-ESP32 layout is:

| Component | Offset |
| --- | ---: |
| Bootloader | `0x1000` |
| Partition table | `0x8000` |
| `boot_app0` | `0xE000` |
| Application | `0x10000` |

The final release process verifies that each range in the factory image exactly
matches the corresponding source binary and that unused gaps contain `0xFF`.

## Not measured without hardware

Free heap after boot, minimum free heap, largest free block over time, task
stack high-water marks, watchdog/reset behavior, live TFT rasterization, actual
Wi-Fi/TLS reachability, Cloudflare acceptance, Stratum/share behavior, and
long-runtime memory stability require the physical target board. No values are
claimed for those measurements.
