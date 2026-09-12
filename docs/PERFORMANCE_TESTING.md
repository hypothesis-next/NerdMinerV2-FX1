# Performance-test methodology

V1.8.3-multipool-perf.5 is an unofficial local release candidate. It does
not claim that one megahash per second has been achieved.

## What is validated on the host

- SHA-256 standard vectors and SHA-256d vectors.
- The byte-exact Bitcoin genesis block header and digest, plus historical block
  headers at heights 1, 100000, and 700000.
- Little-endian 256-bit target comparison at every first-differing byte.
- Fixed-seed differential comparison between the optimized mining SHA and an
  independent reference implementation.
- Exact early-filter acceptance and rejection behavior.
- Stale generation and deliberately corrupted candidate rejection.
- Ten thousand forced exact-target candidates with matching, corrupted, and
  stale variants, including the 32-bit generation wrap boundary.
- Sequential mixed-size nonce ranges without overlaps or gaps.

The normal deterministic run uses five million randomized 80-byte headers.
Host throughput is useful only for comparing host compiler variants; it is not
an estimate of ESP32 throughput.

`tools/run_native_tests.ps1` compiles both native suites from the current
source, so the mining result cannot come from a previously built executable.

## What must be measured on ESP32_2432S028_2USB

1. Record chip revision, CPU frequency, firmware version and power supply.
2. Run at least 30 minutes before recording local hashrate.
3. Compare at least 24 hours of completed nonce accounting with elapsed time.
4. Confirm submitted shares pass the independent validator and are accepted.
5. Compare pool-side estimated work over a statistically meaningful interval.
6. Exercise display changes, Wi-Fi reconnect, rapid new jobs and statistics API
   failures while mining.
7. Record free/minimum heap, task stack high-water marks and watchdog resets.

Only unique, reference-correct nonce attempts count as performance. Displayed
hashrate alone is not proof.

## Compiler experiment

The target's original `-Os` hot routine is 15,955 bytes with a 112-byte stack
frame. Isolated `-O2` and `-O3` generated the same 20,110-byte routine with a
160-byte frame. With no physical timing evidence to justify the increased IRAM
and stack pressure, the release retains `-Os`.

The new classic-ESP32 hardware pipeline was also compiled in isolation. Its
`-Os` hot routine is 642 bytes with a 48-byte frame. Per-function `-O2` grew it
to 781 bytes/64 bytes, and `-O3` to 897 bytes/96 bytes, with more spills. The
candidate therefore retains `-Os` for this routine as well.

## Classic ESP32 hardware-pipeline experiment

The legacy worker exposes all 40 SHA text-register writes around three hardware
compressions for every nonce. The experimental path overlaps 16 second-block
writes with the first compression, eight double-SHA padding writes with the
second compression, and eight next-header writes with the third compression.
Only the remaining eight next-header writes are necessarily exposed after the
digest read.

The first nonce and every subsequent 4096th nonce in each range is recomputed
with the independent reference SHA-256d implementation. Every exact early-
filter hit is also recomputed before it can become a candidate. A mismatch
disables the experimental path for the rest of the boot and causes the entire
range to be recomputed by the retained sequential hardware implementation;
discarded experimental work is not added to the hashrate counters.

This scheduling relies on the classic ESP32 peripheral latching its text input
when START/CONTINUE is issued. The installed ESP-IDF exposes no supported API
for restoring SHA midstate, and host tests cannot prove the peripheral timing.
The path is therefore a local experimental candidate and requires real-board
correctness, throughput, Wi-Fi, TLS and watchdog validation before release.

## Architecture experiment policy

Worker affinity, two-software-worker mode and hardware-midstate restoration are
not selected based on static estimates. Two current software workers cannot
plausibly replace the approximately 300 kH/s hardware contribution, and classic
ESP32 has no supported SHA-state restore operation. Affinity and priority remain
unchanged until measured on hardware.
