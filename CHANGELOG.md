# Changelog

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
