# V1.8.3-FX1 changelog

Unofficial NerdMiner_v2 fork. Original NerdMiner_v2 and BitMaker attribution and the MIT license are retained. No affiliation with or endorsement by those projects or HeliosPool is implied.

- Corrected full 256-bit target comparison and network-target byte order. Added independent reference SHA-256d validation before share submission, including rejection and diagnostics for mismatches.
- Strengthened immutable job/candidate ownership, full-width generation and stale-job handling, unique nonce ranges, partial-range accounting, long-running counters, and the `0xFFFFFFFF` nonce candidate boundary. Corrected a Merkle-root buffer terminator overrun.
- Retained the documented classic-ESP32 SHA-peripheral sequence; repaired physical hardware digest mismatches with required synchronization. Kept the unsafe active-`SHA_TEXT` overlap experiment out of release builds. Improved safe hardware-worker code and TLS/mining coordination without weakening candidate validation.
- Replaced the Public Pool-only dashboard assumption with pool-specific providers and explicit `N/A` behavior for unknown pools. Added HeliosPool support using its deployed `/api/users/<address>` route; no Public Pool fallback is used for Helios or custom hosts.
- Kept HTTPS certificate and hostname verification. Updated Helios trust to the verified ISRG Root X2 chain, retained other providers' CA bundle, and bounded response parsing, caching, stale state and backoff.
- Removed the previously measured software-only hashrate collapse during Helios refreshes by isolating TLS cryptography from the mining SHA peripheral while preserving secure TLS. Corrected dashboard clipping, rendering-memory pressure and startup version-label placement.
- The startup/configuration screen now shows **V1.8.3-FX1** in its free lower strip; the longer internal build identifier remains available for diagnostics.
- Improved reproducible release evidence, application-only flashing safeguards, merged-image layout verification, independent tests and hardware validation.

On one physically tested ESP32_2432S028_2USB, the FX1 source state sustained **436.01 kH/s** over **448.72 seconds** after warm-up, versus that board's approximate 340 kH/s stock baseline. This is a single-board measurement, not a performance promise for other units. The freshly committed release binary differs from that tested binary only in WiFiManager's compile timestamp and derived image checksums; it was not reflashed. See `HARDWARE_VALIDATION.md` and `BINARY_COMPARISON.md`.
