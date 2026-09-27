# Changelog

## V1.8.3-multipool-perf.10-audit.1 - physical hardware candidate

- Uses bounded, documented SDK other-core stalls and raw DPORT reads only
  inside those intervals; retains MEMW and idle-before-SHA_TEXT-write ordering.
- Amortizes SHA ownership over bounded groups; independent candidate validation,
  job generation and completed-nonce accounting remain unchanged.
- Coordinates secure transport with exact software mining fallback, preserving
  network responsiveness without disabling mining or certificate verification.
- Updates Helios to official ISRG Root X2 and a bounded 15-second handshake.
  Uses byte-addressable heap for the memory gate and completion-based retries.
- Reuses the display sprite allocation to avoid TLS-time heap fragmentation;
  fixes vertically clipped pool values without redesigning the screen.
- Corrects the inherited Merkle string terminator's one-byte out-of-bounds write.
- Removes private framework paths from release diagnostic strings.
- The perf.10 checkpoint physically measured 409.09 kH/s with 381 accepted,
  zero rejected submissions and zero SHA validation errors. See the bundled
  report for exact audit.1 binary validation and remaining memory/stability risks.

## V1.8.3-multipool-perf.9-audit.1 - local regression gate corrections

- Keeps job precomputation buffers alive across coordinator iterations.
- Stops ranges at each qualifying candidate and resumes their exact suffix;
  preserves results with bounded backpressure rather than dropping a full queue.
- Snapshots network target and pool difficulty; preserves valid block candidates
  regardless of pool difficulty changes, including equality boundaries.
- Accounts queued completed work on stop/disconnect and synchronizes generation
  transitions with queue clearing. Emits exactly eight nonce hex digits.
- Initializes absent NVS share/block values. Physical validation remains deferred.
- Protects the shared SHA_TEXT window against other SHA algorithms with the SDK
  memory lock for one nonce, plus the all-engine idle wait. Never holds this
  critical section while waiting for result-queue space or validating candidates.
- Uses nonblocking SHA engine acquisition; when TLS owns SHA-256, hashes the
  allocated range with the existing software engine rather than blocking mining.
- Uses full reference hashing for unusually easy jobs outside the exact fixed
  16-zero-bit filter domain; retains normal optimized Bitcoin mining.

## V1.8.3-multipool-perf.9 - local protected-HW performance candidate

- Moves the classic-ESP32 sequential SHA range kernel into IRAM.
- Inlines the official APB pre-read / interrupt-protected DPORT workaround
  into BUSY polling, removing repeated windowed function calls.
- Combines final idle polling and the exact-filter digest-word read under
  one protected interval; inlines an integer-only nonce byte swap.
- Shares one SHA register-window base across the range and control writes.
- Selects per-function `-O2` for the HW kernel only; the SW engine stays `-Os`.
- Separates candidate presence from the valid nonce value `0xFFFFFFFF`.
- Adds emitted-Xtensa instruction / SHA-MMIO model tests for header ownership,
  padding, candidate storage, nonce wrap, cancellation and the peripheral contract.
- Rejects schedule-first, split-compression and alternate-majority SW trials;
  the production software SHA source is unchanged from perf.8.
- No counter/timing, UI, Stratum, wallet, PoolStats, TLS or NTP changes.
- Local builds and tests only. Physical throughput, stability, heap/stack and
  TLS/hardware-SHA coexistence require later real-board validation.

See `docs/PERF9_RESULTS.md` for evidence, experiment decisions and estimates.

## V1.8.3-multipool-perf.8 - local safe-performance candidate

### Changed

- Uses ESP-IDF's documented protected sequence-read form for repeated classic
  ESP32 SHA BUSY polling.
- Keeps the SHA_TEXT window base in one address register, removing repeated
  per-word address materialization without writing while the engine is busy.
- Precomputes the job-constant contributions to software-SHA schedule words
  W18 and W19.
- Retains the documented sequential hardware pipeline and the existing fully
  unrolled software engine; rejected compact and partial-unroll experiments
  are not present in production source.

### Validation status

- Five million differential SHA cases pass with zero mismatches.
- The `ESP32_2432S028_2USB` target builds successfully with unchanged static
  RAM usage and unchanged 112-byte software-SHA stack frame.
- Throughput effects remain **REQUIRES REAL HARDWARE**; no 600--800 kH/s claim
  is made.

## V1.8.3-multipool-perf.6 - local correctness candidate

### Changed

- Disabled the perf.5 SHA_TEXT-overlap experiment in normal builds because the
  classic ESP32 hardware contract requires the shared text window to remain
  untouched while the engine is busy.
- Retained the experiment behind the explicit development-only
  `NERDMINER_EXPERIMENTAL_SHA_TEXT_OVERLAP=1` build macro.
- Safely overlaps only the nonce byte swap in CPU registers and waits for the
  final hardware LOAD to become idle before reading the digest.
- Corrected the unsupported SW/HW baseline decomposition in the performance
  methodology; 340--350 kH/s is measured only as a combined value.

### Validation status

- Five million differential SHA cases and the full native mining-validation
  suite pass with zero mismatches.
- A clean `ESP32_2432S028_2USB` target build succeeds with the experimental
  SHA_TEXT overlap absent from the release binary.
- Physical throughput and long-runtime behavior remain **REQUIRES REAL HARDWARE**.

## V1.8.3-multipool-perf.5 - local performance candidate

### Changed

- Added a classic-ESP32 hardware-SHA register-fill pipeline that overlaps text
  preparation with the three required compression operations per nonce.
- Moved the compact hardware hot loop to IRAM and retained `-Os` after isolated
  `-O2`/`-O3` variants increased code size, stack, and spills.
- Reduced the normal exact-filter digest read to the framework's SMP-safe
  single-register path; full digest reads remain rare.
- Extended the native test runner so a fresh invocation compiles and executes
  both the PoolStats suite and the five-million-case mining validation suite.

### Correctness guard

- The original sequential hardware miner remains compiled as a permanent
  fallback. The first nonce and every 4096th nonce are independently recomputed,
  as is every early-filter hit. A mismatch disables the pipeline and recomputes
  the complete range sequentially before any work is counted.
- Candidate snapshots, full-width generation checks, exact target comparison,
  Stratum submission, wallet behavior and the visible UI are unchanged.

### Validation status

- Five million optimized-vs-reference software SHA-256d cases pass with zero
  mismatches; target, historical header, forced-candidate, stale-generation and
  nonce-range tests also pass.
- The target firmware builds for `ESP32_2432S028_2USB`. The new peripheral
  schedule and its physical hashrate remain **REQUIRES REAL HARDWARE**.

## V1.8.3-multipool-perf.4 - local release candidate

### Changed

- Added the official self-signed GTS Root R4 certificate as a dedicated,
  provider-specific HeliosPool HTTPS trust anchor.
- Kept the generic Mozilla CA bundle for Public Pool and compatible providers.
- Changed the statistics-task SNTP server from `europe.pool.ntp.org` to the
  geographically distributed `pool.ntp.org` service.
- Made the pre-TLS UTC clock gate deterministic and host-testable.

### Validation status

- The committed PEM exactly matches the official `https://pki.goog/roots.pem`
  GTS Root R4 entry and passed subject, issuer, self-signature, CA constraint,
  validity, fingerprint, and checksum verification.
- A desktop TLS client using only this root validated
  `stats-btc.heliospool.com`; an incorrect hostname was rejected. The live
  snapshot route returned its expected missing-address HTTP 400 response.
- Native PoolStats tests and the five-million-case mining/SHA regression suite
  pass. The clean `ESP32_2432S028_2USB` build uses 55,700 bytes of static RAM
  and 2,144,321 bytes of application flash according to PlatformIO.
- ESP32 TLS, SNTP, display, memory, watchdog, and recovery behavior require a
  later combined physical-hardware validation cycle.

## V1.8.3-multipool-perf.3 - 2026-09-11

Follow-up hotfix for verified HTTPS pool statistics on physical hardware.

### Fixed

- Start non-blocking system time synchronization after Wi-Fi connects and defer
  HTTPS statistics requests until the ESP32 clock is valid for certificate
  verification. NTP failure remains isolated from mining.

## V1.8.3-multipool-perf.2 - 2026-09-11

Hardware-validated hotfix for the HeliosPool statistics task.

### Fixed

- Increased the isolated PoolStats task stack from 10 KiB to 16 KiB. The
  previous allocation overflowed during the classic ESP32 mbedTLS entropy/TLS
  setup path, causing a stack-canary panic and reboot loop after Wi-Fi connected.

### Preserved

- Mining, Stratum, independent SHA-256d validation, display layout, pool
  providers, and performance architecture are unchanged.

## V1.8.3-multipool-perf.1 - 2026-09-11

Performance and correctness test release based on V1.8.3-multipool.1.

### Added

- Independent, permanently enabled reference SHA-256d validation before every
  locally generated share submission.
- Deterministic native SHA, target-boundary, early-filter, stale-generation,
  candidate-corruption, and nonce-range tests.
- Exact 80-byte candidate header snapshots and validation-error accounting.

### Changed

- Corrected the byte-exact little-endian 256-bit network-target comparison.
- Corrected network-target conversion so all 32 bytes, rather than only eight
  byte pairs, are converted to the little-endian representation used by mining.
- Replaced the eight-bit worker cancellation generation with a full 32-bit
  atomic generation.
- Normalized completed nonce counts after every returned batch so the
  sub-million counter cannot overflow during a long interval without a new job.
- Widened the total-kilohash snapshot to 64 bits for long-running devices.
- Build and package filenames now use the firmware's explicit unofficial
  version identifier.

### Deliberately retained

- The visible UI and its refresh behavior are unchanged.
- The current one-hardware-worker plus one-software-worker mining architecture
  is retained. Compiler, affinity, dual-software, and hardware-midstate changes
  require physical timing before they can be selected safely.

### Validation boundary

- A successful build or host benchmark is not a physical ESP32 hashrate
  measurement. Hardware throughput, watchdog, Wi-Fi, and pool validation remain
  required.

## V1.8.3-multipool.1 - 2026-09-10

Unofficial fork based on NerdMiner_v2 V1.8.3.

### Added

- Central pool registry with normalized hostname matching and friendly names.
- Generic pool statistics snapshot and provider interface.
- HeliosPool provider using the compact user snapshot endpoint.
- Separate low-priority statistics service with last-attempt/last-success
  tracking, stale state, response-size limits, timeouts, and failure backoff.
- Mozilla-derived ESP x509 certificate bundle and third-party notice.
- Native deterministic tests and sanitized JSON fixtures.
- Faithful display render mocks for current, unavailable, and stale states.

### Changed

- The lower dashboard is rendered dynamically instead of using fixed
  Public-Pool.io branding.
- The remote metric label is `Best Ever`, matching the selected Helios field.
- Unknown pools now show their configured hostname and `N/A`; they do not fall
  back to the Public Pool API.
- Remote metric values use fixed-size buffers instead of display-frame String
  allocation.

### Preserved

- The upstream Stratum/mining core and local mining statistics behavior.
- Existing supported Public Pool-compatible integrations and TESTNET handling.
- Upstream MIT license and authorship notices.

### Known limitation

- HeliosPool is protected by Cloudflare. Successful access by the classic ESP32
  TLS/HTTP stack must be confirmed on physical hardware; TLS verification is
  never disabled.
