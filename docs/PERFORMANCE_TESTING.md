# Performance-test methodology

V1.8.3-multipool-perf.3 is an unofficial correctness-first test build. It does
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

## Architecture experiment policy

Worker affinity, two-software-worker mode, hardware-midstate restoration and
other SHA-peripheral changes are not selected based on static estimates. Each
must beat the hybrid baseline on physical hardware while producing identical
reference-validated work and preserving Wi-Fi, display and watchdog stability.
