# V1.8.3-multipool-perf.6 local performance-candidate notes

This is an unofficial NerdMiner_v2 performance and correctness test build. It
is not an official or endorsed release of NerdMiner, BitMaker-hub, HeliosPool,
or another pool operator.

This is a locally validated performance candidate. It has not yet been flashed
or validated on physical ESP32 hardware. It accumulates the earlier correctness,
PoolStats stack, and Helios-specific TLS trust-anchor corrections.

HeliosPool HTTPS uses the official self-signed GTS Root R4 certificate while
Public Pool and compatible providers retain the general Mozilla CA bundle.
Hostname and certificate validation remain enabled; there is no insecure or
HTTP fallback. The statistics task now uses `pool.ntp.org` and does not attempt
HTTPS until the system UTC clock is plausibly valid. Any NTP, TLS, API, or JSON
failure remains isolated from Stratum mining.

This revision increases the isolated PoolStats task stack from 10 KiB to 16 KiB
after physical classic-ESP32 testing demonstrated a stack-canary panic in the
mbedTLS entropy/TLS connection path. It also synchronizes the ESP32 system clock
before verified HTTPS statistics requests. Mining and UI behavior are unchanged.

The release preserves the V1.8.3-multipool.1 UI and hybrid SW+HW architecture.
It fixes exact block-target comparison, gives workers a full-width atomic job
generation, snapshots the exact candidate header, corrects full 32-byte target
endianness, and independently recomputes SHA-256d before every locally generated
share submission. A stale or mismatched candidate is never submitted. Completed
hash accounting now uses a synchronized 64-bit total snapshot.

Local release-candidate validation confirmed that the committed PEM is
byte-identical to the official Google Trust Services root, that a desktop TLS
client trusts the current Helios endpoint using only that root, and that an
incorrect hostname is rejected. The live route returned its expected
missing-address response. These desktop results do not prove the handshake on
the older ESP32 mbedTLS stack; that remains part of the later combined hardware
test.

Five million fixed-seed randomized headers passed differential host testing,
including 73 exact early-filter passes. SHA-256d vectors include the Bitcoin
genesis header and historical headers at heights 1, 100000, and 700000. This is
a correctness result, not a physical ESP32 hashrate measurement.

The perf.5 classic-ESP32 SHA text-register overlap is disabled by default. The
installed ESP-IDF explicitly requires all SHA engines to be idle before
SHA_TEXT is modified, and neither its HAL nor the hardware manual provides a
safe input-latched point during BUSY. Sampled validation could not rule out a
rare false-negative filter result. Perf.6 therefore uses the documented
sequential path, overlaps only the CPU-local nonce byte swap, and waits for the
final LOAD operation before reading the digest. The old experiment remains
available only through an explicit development-build macro.

The only physical baseline is 340--350 kH/s combined. Source comments and
target disassembly support approximately 35--45 kH/s for the software worker.
Residual attribution suggests approximately 295--315 kH/s for the production
hardware worker, but the firmware has no per-worker physical counters, so that
split remains an estimate rather than a measurement.

Per-function `-O2` and `-O3` variants were rejected because they increased IRAM,
stack frames and spills. Fixed affinity, two-software-worker, unsupported
hardware-midstate restoration and hand-written assembly were not selected
without physical evidence. This release does not claim 700, 800, or 1000 kH/s.
Actual useful throughput and long-runtime stability require testing on
ESP32_2432S028_2USB hardware.

Use the factory image at offset `0x0000` for a clean development-device flash.
Use the application image at `0x10000` only when the device already has the
compatible classic-ESP32 bootloader and `huge_app.csv` partition table. Preserve
the V1.8.3-multipool.1 factory image as rollback firmware.
