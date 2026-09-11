# Changelog

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
