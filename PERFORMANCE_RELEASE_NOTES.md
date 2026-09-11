# V1.8.3-multipool-perf.1 test release notes

This is an unofficial NerdMiner_v2 performance and correctness test build. It
is not an official or endorsed release of NerdMiner, BitMaker-hub, HeliosPool,
or another pool operator.

The release preserves the V1.8.3-multipool.1 UI and hybrid mining architecture.
It fixes exact block-target comparison, gives workers a full-width atomic job
generation, snapshots the exact candidate header, corrects full 32-byte target
endianness, and independently recomputes SHA-256d before every locally generated
share submission. A stale or mismatched candidate is never submitted. Completed
hash accounting now uses a synchronized 64-bit total snapshot.

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
