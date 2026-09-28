# V1.8.3-FX1 hardware validation

This is an unofficial NerdMiner_v2 release candidate, tested on **one** ESP32_2432S028_2USB (classic ESP32-D0WD-V3 rev 3.1, 240 MHz, 4 MB). Results do not establish multi-board or multi-day reliability. No Wi-Fi credentials or mining address are included.

## PHYSICALLY TESTED: FX1 source state, startup/UI smoke test

The two startup UI edits were physically smoke-tested before being committed, with no subsequent source edit. The tested application SHA-256 was `8645a861506ee324b51f718a1377fe3d40e6ec0e78d9df1763039342f35b8cd5`; only the application partition at `0x10000` was written and verified. The existing bootloader, partition table, NVS and filesystem were not erased or replaced.

The tested board loaded its saved configuration and automatically connected to Wi-Fi and its configured Helios Stratum service. No setup portal appeared. Subscribe, authorize, difficulty, notify, worker activity and share submission were observed. The user visually confirmed the exact `V1.8.3-FX1` startup label, normal screen transition, readable Helios values, and correct operation after turning the backlight off and on once.

The 510.2-second capture included a 60-second warm-up. From device-reported **completed nonce counts** and monotonic elapsed time after warm-up:

| Metric | Observed result |
|---|---:|
| Measurement interval | 448.72 s |
| Completed hashes | 195,645,712 |
| Independently calculated average | 436.008 kH/s |
| Displayed average | 435.859 kH/s |
| Approximate stock baseline on the same board | 340–345 kH/s |
| Accepted / rejected shares | 227 / 0 |
| Candidate SHA-256d validation failures | 0 |
| Helios completed refreshes | 6 HTTP 200 / 6 parsed snapshots |
| Hardware-to-software fallback during those refreshes | 0 microseconds / 0 nonces |
| Observed crashes / watchdogs / reconnects | 0 / 0 / 0 |
| Minimum reported free heap during TLS | 4,964 bytes |

Five warm refreshes averaged about 401.5–415.2 kH/s during their request windows; the initial cold refresh averaged about 388.9 kH/s. The old approximately 37 kH/s software-only collapse did not recur. Short approximately five-second windows varied and some fell below 400 kH/s; this report does not claim constant 400 kH/s. Helios Best Ever, Workers and Total Hash Rate remained numeric throughout the six observed refreshes. The user confirmed no clipping or freeze. The on-device build continued running after capture.

## PHYSICALLY TESTED: earlier extended, functionally identical core

Before the final startup-label-only edit, the same board ran the immediately preceding `V1.8.3-multipool-perf.11-rc.2` candidate. After warm-up it completed **525,662,504 hashes in 1199.413 s**, yielding **438.266 kH/s**; the displayed average was **438.425 kH/s**. It completed **14 Helios HTTP 200 refreshes** and observed **433 accepted / 0 rejected shares**, zero candidate-validation failures, and no observed crashes, watchdogs or reconnect loop. Warm refresh averages were approximately 420.6–428.4 kH/s; the lowest observed warm five-second window was approximately 394.5 kH/s. Its minimum reported free heap was **4,608 bytes**. The final UI commit did not change this mining/TLS/PoolStats core.

The unchanged production hardware-SHA path also passed **2,979,747 physical comparisons** during development: 1,800,099 full SHA-256d digests and 1,179,648 unforced exact-filter decisions, with zero mismatches. This includes known headers and nonce boundaries. These comparisons were development diagnostics, not 2.98 million live submitted shares.

## HOST TESTED / VERIFIED BY BUILD

- Independent reference-SHA differential suite: **5,000,000 deterministic cases**, zero mismatches, including known Bitcoin headers, target/endian and early-filter boundaries, candidate corruption, generation/stale behavior and nonce-range/accounting tests.
- Clean `ESP32_2432S028_2USB` build from commit `33bd54f9077f7b31dd40ee776637dfc1f1df17c9`: PASS. Static RAM 56,380 bytes; reported application flash 2,154,693 bytes; IRAM 110,863 text + 1,027 vectors.
- The **final clean committed-build application** has SHA-256 `1cb16e3586a2a7cfd5cfc666c21b4102ca9709c81f71dd2fc09052cf0552a3e5`. It was **not** flashed during packaging. Byte comparison against the physically tested FX1 application found only WiFiManager's compile timestamp and derived ELF/image checksums changed. See `BINARY_COMPARISON.md`.
- Factory-image bootloader, partition table, `boot_app0`, and application were byte-compared against the clean build at `0x1000`, `0x8000`, `0xE000`, and `0x10000`, respectively.
- Source retains verified Helios TLS with CA and hostname checks; no insecure TLS fallback. Six live HTTPS responses succeeded in the FX1 smoke test. This is not a security audit of every third-party dependency.

## STATIC / KNOWN LIMITATIONS

- Low TLS-time free-heap margin (approximately 4.6–5.0 KiB observed) warrants continued monitoring; the short smoke test and earlier twenty-minute run cannot rule out rare low-memory failure or multi-day fragmentation.
- The corrected Helios `/api/users/<address>` route stayed available during these tests, but the earlier roughly nine-hour failure scenario has **not** been repeated for nine hours after the fix. External API and CA changes remain possible; statistics failure is designed to remain secondary to mining.
- Pool-side five-minute/share-derived hashrate can differ sharply from completed-hash accounting; it is not a direct CPU-throughput measurement. Best Ever is an account-lifetime submitted-share metric, whereas local Best Difficulty follows the device's accepted-share state.
- Only one board, network, account and short backlight off/on cycle were tested. Cold TLS connection can cost more than warm refresh. This release does not promise a specific hashrate or stats availability on other devices or networks.

**Release evidence conclusion:** The FX1 source state passed the physical smoke test, and the final committed rebuild has no untested functional source change. The exact final clean-build bytes have not themselves been physically retested.
