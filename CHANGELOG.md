# Changelog

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
