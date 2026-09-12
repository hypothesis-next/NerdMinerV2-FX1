# V1.8.3-multipool-perf.4 local release-candidate notes

This is an unofficial NerdMiner_v2 performance and correctness test build. It
is not an official or endorsed release of NerdMiner, BitMaker-hub, HeliosPool,
or another pool operator.

This is a locally validated release candidate. It has not yet been flashed or
validated on physical ESP32 hardware. It accumulates the earlier correctness
and PoolStats stack fixes with a Helios-specific TLS trust-anchor correction.

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

The release preserves the V1.8.3-multipool.1 UI and hybrid mining architecture.
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

Speed-oriented compiler, fixed-affinity, two-software-worker and hardware
midstate variants were not selected without physical timing proof. Consequently
this release does not claim one megahash per second. Actual throughput and
long-runtime stability require testing on ESP32_2432S028_2USB hardware.

Use the factory image at offset `0x0000` for a clean development-device flash.
Use the application image at `0x10000` only when the device already has the
compatible classic-ESP32 bootloader and `huge_app.csv` partition table. Preserve
the V1.8.3-multipool.1 factory image as rollback firmware.
