# V1.8.3-multipool-perf.11-rc.2 hardware-test candidate

The whole-response drain deadline is 90 seconds after a physical rc.1 run
reproduced a valid historical tail exceeding 30 seconds. Body size/read bounds
remain enabled; the correction avoids unnecessary cold TLS reconnections.

Unofficial NerdMiner_v2 modification; no upstream or pool endorsement.
This candidate updates Helios to `/api/users/<address>` and keeps authenticated
HTTPS open across bounded, completion-based refreshes. It uses the official
software SHA ALT mode only for statistics-task TLS contexts, with bounded CPU
windows outside the documented hardware miner's protected SHA/DPORT intervals.
No hashing/target/ownership/submission validation is weakened.

The target consistently builds matching official TLS runtime sources with the
supported 1024-byte outgoing-record limit; incoming records remain 16 KiB.
ISRG Root X2, hostname verification, valid-UTC gating, cache and backoff remain.
Other providers keep the generic trust bundle; unknown hosts make no API request.

The protected digest kernel passed 2,979,747 physical comparisons without a
mismatch and five million host differential cases. Bulk-read and no-CPU-window
variants were rejected after measured throughput/watchdog regressions. Exact
final-binary throughput, refresh minima, shares and heap measurements belong to
the accompanying hardware validation report. See `docs/HELIOS_REFRESH_FIX.md`.

Use a verified application-only update to preserve an existing configuration.
The factory image and manifest are fresh-install options, not preserving updates.
The upstream MIT license and bundled TLS runtime Apache 2.0 notices are retained.
Multi-hour/multi-day validation remains pending; initial or reconnected full TLS
handshakes still cost CPU time and briefly retain the last complete LCD frame.

## Historical perf.10-audit.1 candidate notes

Unofficial NerdMiner_v2 modification; no upstream or pool-operator endorsement.

- Sequential classic-ESP32 SHA obeys idle-before-write and retains MEMW after
  commands. Bounded SDK other-core stalls protect DPORT access; the unsafe
  active-SHA_TEXT overlap experiment remains disabled.
- Independent candidate validation, generation ownership and completed-nonce
  accounting remain enabled. TLS activity selects the existing exact software
  fallback instead of starving secure transport on the other core.
- Helios uses the official ISRG Root X2 with hostname/certificate verification,
  a bounded 15-second handshake and unchanged snapshot endpoint. Its current
  certificate chain is not anchored at the historical GTS Root R4.
- Reusable display storage avoids transient large sprite allocation failures;
  pool values now honor their intended vertical centering. No screen redesign.
- Retry delay begins after request completion; byte-addressable heap is used
  for the TLS memory gate. Other provider selection and generic trust remain.
- audit.1 removes private framework paths from compiled diagnostic strings.

On the physical test board the perf.10 implementation completed 490,454,000
hash attempts in 1,198.891 seconds after warm-up: 409.09 kH/s, with 381 accepted
submissions, none rejected, and no SHA validation/reset/reconnect failure.
For the exact audit.1 binary, see the accompanying hardware-test report.
These are limited single-board observations, not multi-day stability proof.

## Historical perf.9 local-only notes (not current release validation)

This is an unofficial modification of NerdMiner_v2, not an endorsed release
of BitMaker-hub, NerdMiner, HeliosPool, or any pool operator.

LOCAL VALIDATION ONLY. No physical board was accessed or flashed for this
revision. No measured hashrate increase or hardware stability claim is made.
Keep the preserved perf.8 release as the safe local comparison point.

## Retained mining changes

- A small sequential classic-ESP32 SHA range kernel now runs in IRAM.
- BUSY polling uses an inlined version of ESP-IDF 4.4.6's protected APB pre-read
  workaround, with interrupt-level restoration and memory barriers intact.
- Final idle polling and the exact-filter word read share a protected interval.
- Integer-only nonce byte swapping no longer calls a library per nonce.
- Input fills and control stores share a range-local SHA register-window base.
- The HW kernel alone uses per-function -O2. Whole-firmware and SW flags stay -Os.
- Candidate presence has its own boolean; nonce 0xFFFFFFFF is no longer
  incorrectly interpreted as absence of a candidate.

Each HW nonce still performs three compressions, two LOADs, 40 input stores
and five idle checks. No SHA_TEXT write is allowed while the engine is active.
The legacy unsupported overlap experiment is not enabled in the release.
The software SHA engine is byte-for-byte unchanged from perf.8.

The independent reference SHA-256d submission gate, target comparison, full
generation, stale protection and completed-work accounting are retained.
Stratum serialization, wallet/reward behavior, UI, PoolStats, Helios TLS,
certificates, NTP and configuration were not changed.

## Validation and experiments

See docs/PERF9_RESULTS.md for exact code sizes, stack frames, native tests,
emitted-instruction model results and rejected experiment evidence.

The emitted-Xtensa test is a strict host instruction/MMIO model, not an ESP32
hardware emulator. SHA compression is supplied by an independent desktop SHA
implementation; the model exercises the actual compiled CPU instructions,
padding, nonce endian handling, filter branch, candidate storage, cancellation,
range wrap and idle-before-write / protected-read contract.
Its instruction counts are not physical cycle measurements.

Schedule-first and split-compression software trials passed five million
deterministic cases each but increased stack pressure without convincing speed
evidence. An equivalent-majority trial produced the same Xtensa hot function
and was rejected. None of these trials remains in production source.

## Performance expectation, not measurement

The only physical baseline is the user's approximately 340 kH/s combined.
The inferred split is about 40 kH/s SW plus 300 kH/s HW, not independently
measured worker rates. The best engineering estimate is approximately 385 kH/s
combined; realistic 370--400, conservative 340--365, optimistic about 420 kH/s.
These are uncertain static estimates, not promised results or acceptance gates.

Removed calls partly overlap mandatory hardware latency, so instruction
reduction cannot be converted directly into hashrate. There is no credible
evidence for 700--800 kH/s from the retained architecture, nor proof that this
candidate is the absolute hardware optimum.

## Deferred hardware validation

Measure unique completed work over long runs; accepted/rejected/stale shares;
the actual SW/HW split; task stack high-water marks; minimum/free/largest heap;
watchdog resets; Wi-Fi/Stratum responsiveness; and TLS/SHA-lock coexistence.
Validate under rapid new jobs and API errors with normal active display use.
Do not treat the host model as physical chip/errata validation.

## Installation later

Factory image: offset 0x0000. Included components: bootloader 0x1000, partition
table 0x8000, boot_app0 0xE000, application 0x10000.
Application-only image: offset 0x10000, only with an already compatible
classic-ESP32 bootloader and huge_app.csv layout.
No flashing is authorized by these notes. Preserve perf.8 and the known-good
multipool.1 rollback artifacts. MIT licensing and upstream notices remain;
the adapted DPORT workaround additionally retains its Apache 2.0 notices.
