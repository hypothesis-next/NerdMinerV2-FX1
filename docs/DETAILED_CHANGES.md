# V1.8.3-FX1: complete stock-to-release engineering change record

This is the engineering-level record for the unofficial **V1.8.3-FX1 RC1**
source. The concise user release history is [CHANGELOG.md](../CHANGELOG.md).
“Stock” below means the official BitMaker-hub/NerdMiner_v2
`nerdminer-release-V1.8.3` tag, commit
`a26865f7cdd9ac1a81c5b0a7c355e23cf4a1d568`, tree
`93d4be15781fd3f0140d928cbe31d28e86713cb4`. The repository was fetched
directly from [upstream](https://github.com/BitMaker-hub/NerdMiner_v2) and the
tag/ref and Git tree were checked locally. This is the exact public release tag,
not today's upstream default branch.

## Baseline provenance and scope

The fork's first local commit, `53be303` (“Import NerdMiner_v2 V1.8.3
baseline”), has tree `cfe0319c3264fc4f52b3462e1ee32a71f00aef1c`.
**It is not identical to the official tag:** 26 files differ. Four of those
files also changed later in FX1. The imported snapshot contains additional
LilyGo T3 V1, M5 Cardputer Advanced and Spotpear/SSD1306 support, some
build/default-pool settings, and font/readme formatting differences. It
omitted upstream editor settings and two historical flash-tool logs. The
precise upstream commit from which that import was assembled was not
recoverable by an exact tree match in upstream `main` history. Therefore the
authoritative comparison here is **official tagged V1.8.3 → final FX1**, and
inherited import differences are identified rather than misrepresented as
new FX1 engineering.

The frozen physically tested FX1 application is
`NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-application.bin`, SHA-256
`1cb16e3586a2a7cfd5cfc666c21b4102ca9709c81f71dd2fc09052cf0552a3e5`.
The internal build identifier remains
`V1.8.3-multipool-perf.11-rc.2`; only the public startup label is
`V1.8.3-FX1`. Documentation, packaging and web-install changes do not
alter that binary. Unless stated otherwise, physical observations cover one
ESP32_2432S028_2USB, not every supported upstream board.

## Mining and Bitcoin correctness

- **`src/mining.cpp`:** `JobRequest` now snapshots the complete
  job-relevant header, nonce range, generation, pool difficulty and target;
  `JobResult` retains an 80-byte candidate header and ownership metadata.
  `JobPush`, `MiningJobStop`, the two workers and
  `runStratumWorker` use full-width atomic generations so late results cannot
  be attributed to a new job. Range prefixes already completed before a
  candidate or cancellation are accounted for once and the untouched suffix
  resumes without a gap. `MiningRangePolicy.h` holds the narrow range and
  nonce formatting rules. The candidate sentinel cannot hide a real
  `0xFFFFFFFF` nonce. These changes address stale, lost, duplicated or
  misattributed Bitcoin work; they are not UI counter adjustments.
- **Reference validation:** `src/crypto/ReferenceSha256.{h,cpp}` contains
  an independent SHA-256d implementation. Before any candidate is sent,
  `runStratumWorker` reconstructs the exact 80-byte header, checks its
  reference digest, compares all 256 target bits in Bitcoin byte order,
  verifies the generation/job owner, and only then submits. A mismatch is
  critical and not counted as a valid share. Very easy targets can use the
  exact reference range path rather than an unsafe fixed-word shortcut.
- **Target and submission:** `checkValid`/target handling in
  `src/utils.{h,cpp}` and the mining worker use corrected endian conversion
  and full-width comparison. `tx_mining_submit` in `src/stratum.cpp`
  formats a fixed eight-hex-digit nonce and reports failure if the entire
  message was not written; it retains the selected worker name, job ID,
  extranonce2 and nTime. `parse_extract_id` only received a trailing newline
  cleanup. No reward destination is substituted.
- **Counters and persistence:** `addCompletedHashes` and
  `completedHashesSnapshot` synchronize the 64-bit total on the 32-bit ESP32;
  `totalKHashes` and monitor conversions avoid the old long-runtime wrap.
  Only actually completed nonce prefixes are added. `restoreStat`,
  `saveStat`, `resetStat` initialize absent NVS values and preserve the
  opt-in saved-statistics behavior. Worker/range accounting is independent of
  remote pool-side hashrate estimates.
- **Merkle memory safety:** `calculateMiningData` in `src/utils.cpp`
  no longer writes a terminator one byte beyond a fixed buffer. It keeps
  the same Merkle result and protects adjacent memory.

## Classic ESP32 SHA hardware and scheduling

- `src/crypto/ClassicEsp32ShaAccess.h` retains Espressif's Apache-licensed
  DPORT APB pre-read/interrupt protection where needed and uses documented
  other-core-stall access only inside the SDK's protected interval. An Xtensa
  `MEMW` after SHA START/CONTINUE/LOAD is essential: physical testing found
  that a fast poll could otherwise see pre-command idle and read a wrong
  digest. The first perf.9 physical candidate failed independent validation;
  the corrected path passed extensive on-device comparison. No release code
  writes `SHA_TEXT` while BUSY.
- `minerWorkerHw` and its helper functions in `src/mining.cpp` retain the
  safe three-compression SHA-256d sequence and original sequential fallback.
  The optimized range kernel reuses register addresses, minimizes protected
  digest reads for ordinary noncandidates, groups ownership safely and uses
  exact candidate/filter checks. `ClassicShaDiagnostics.h` supplies
  development-only full-digest/filter comparisons; it is not a synthetic
  hashrate path. `ShaResourcePolicy.h` encodes bounded CPU arbitration.
- The software worker still performs real Bitcoin-specific SHA-256d work.
  `src/ShaTests/nerdSHA256plus.cpp` changes are limited to exact schedule
  precomputation and candidate-filter behavior; it is not a generic hash
  substitution. `nerdSHA_HWTest.cpp` follows the corrected protected
  hardware-read ordering. Two-worker and compiler variants were compared
  locally, but unsupported or slower alternatives were not installed as the
  public default.
- The hardware worker and statistics TLS coexist through documented
  synchronization. TLS cryptography uses the SDK's software SHA ALT path for
  the statistics task, avoiding a whole-request hardware-miner shutdown.
  Bounded CPU windows occur only after peripheral/DPORT locks are released.
  Stratum, candidate validation and the wallet are not owned by PoolStats.

## Pool providers, API and TLS

- `src/monitor.{h,cpp}` no longer performs a blocking HTTP request from
  the display getter or falls back to Public Pool for unknown hosts.
  `getPoolData` reads a snapshot from the separate PoolStats service.
  `PoolRegistry` normalizes host/worker names; `PoolStatsProvider` selects
  only recognized providers. Public Pool-compatible hosts retain their
  established API behavior. Unknown hosts show honest unavailable remote
  metrics while mining continues.
- `PoolStatsTypes`, `PoolStatsPolicy` and `PoolStatsService` define a
  bounded low-priority refresh task, cache, stale/error state, retry/backoff,
  HTTP 429 policy and thread-safe display snapshot. Invalid UTC blocks HTTPS
  until global SNTP time becomes plausible; the server is `pool.ntp.org`.
  Failures cannot stop the Stratum worker.
- Helios originally used a query-style snapshot route that later returned
  HTTP 400 for a checksum-valid address. The deployed user page requests
  `/api/users/<address>`. `HeliosUserPrefix.h` and
  `HeliosUserCapture.h` retain only the compact first user object while a
  bounded historical tail is drained. `PoolStatsParsers` extracts
  account `user.stats[0].bestEver`, `hashrate5m` and active worker
  timestamps, with malformed/missing values handled honestly.
- `StatsTlsClient` and `PoolStatsProvider` use verified HTTPS,
  hostname checks, a dedicated current Helios ISRG Root X2 trust anchor,
  and the generic Mozilla/certifi bundle for compatible other providers.
  The older GTS Root R4 PEM is retained for provenance but is not the current
  Helios anchor. No `setInsecure()`, HTTP downgrade or Public Pool fallback
  was added. A fully drained, verified keep-alive connection is reused;
  a 512 KiB response cap, 90-second whole-response deadline and ordinary
  socket timeouts prevent unbounded reads.
- Matching, unmodified mbedTLS 2.28.4 TLS runtime files are vendored under
  `vendor/mbedtls-tls` to use a supported 1024-byte outbound record limit
  with 16 KiB inbound records. `platformio.ini` selects these files only
  for the tested classic-ESP32 target and checks SDK/header compatibility.
  The vendor license/origin and ESP-IDF DPORT attribution are retained.

## Display, memory, build and release

- `src/drivers/displays/esp23_2432s028r.cpp` keeps the established CYD
  screen style, adds a bounded Helios/provider panel, corrects its vertical
  clipping, and places `V1.8.3-FX1` clear of the QR/logo/status bounds.
  `display.{h,cpp}` lends reusable rendering scratch during cold TLS setup
  and restores complete-frame rendering afterwards; backlight off/on does
  not stop statistics. T-HMI and WT32 display changes adapt the same
  pool snapshot without redesigning the broader UI.
- The imported snapshot's LilyGo/Cardputer/Spotpear/SSD1306 and I2C changes
  are inherited pre-FX1 source differences from the official tag. Their
  original rationale is **not conclusively recoverable from repository
  history**; only the CYD target above has FX1 physical validation. Font
  license/readme line-ending and format changes are also inherited and
  do not change the tested firmware.
- `platformio.ini`, `auto_firmware_version.py`,
  `post_build_merge.py`, certificates, tests and `tools/` scripts support
  reproducible target selection, native correctness tests, factory merge,
  codegen inspection, checksum/source packaging and panel render checks.
  The application binary is frozen and was not rebuilt for documentation.
- `docs/index.html`, its update/factory manifests, images and
  `docs/release/` form a lightweight ESP Web Tools site. The recommended
  update manifest has **one** part: the exact public application at
  `0x10000`; it writes no bootloader, partition table, NVS or filesystem.
  The factory image at `0x0000` is intentionally secondary and warned as
  configuration-destructive. Manual flashing and rollback instructions,
  checksums, licenses and validation evidence are packaged separately.
- The public branch omits the inherited `bin/` directory of obsolete
  prebuilt stock images and a Windows flashing-tool snapshot. Those files
  are not required to build FX1 or use the verified web/manual installers;
  old tool settings contained machine-local paths. The original local
  development branch is preserved privately, while the public branch
  removes this directory from every published commit, not only HEAD.

## Confirmed bug inventory

Each row distinguishes the old or intermediate behavior from the final fix.
“Intermediate” means a defect found while developing FX1, not an allegation
that official stock had that specific new defect.

| Confirmed issue | Stock/intermediate behavior and impact | Root cause and final fix | Validation |
|---|---|---|---|
| Target comparison and byte order | Stock could evaluate only a partial/wrong-order target; valid work might be missed or invalid work considered | Full 256-bit network-order comparison and independent reference digest | HOST TESTED: target/endian boundaries and 5,000,000 differential cases |
| Candidate integrity | Optimized output alone could reach submission | Exact 80-byte immutable snapshot is independently SHA-256d checked before any submit | HOST TESTED: corruption/known-block cases; PHYSICALLY TESTED: zero validation errors in retained runs |
| Nonce `0xFFFFFFFF` | Sentinel collision could hide that real candidate | Candidate flag/ownership separated from nonce value | HOST TESTED: nonce boundaries |
| Stale generation/job | Truncated or mutable job ownership could attribute late work to another notification | Atomic full-width generation and snapshot/job check on result and submission | HOST TESTED: rapid-job/stale tests |
| Nonce range and accounting | Interrupted batches/candidates risked skipped suffixes or overcounting | Exact completed-prefix continuation; 64-bit synchronized counter | HOST TESTED: range overlap/gap/partial tests; PHYSICALLY TESTED: counter-derived rate agrees with display |
| Merkle terminator | Stock fixed buffer could receive a one-byte out-of-bounds terminator | Sized destination and bounded terminator placement | SOURCE VERIFIED; no dedicated host regression test recorded |
| HW SHA command visibility | Intermediate fast poll read pre-command idle; candidate digest failed on ESP32 | Retained documented DPORT protection plus required `MEMW` after command | PHYSICALLY TESTED: first perf.9 mismatch isolated; 2,979,747 corrected comparisons, zero mismatches |
| Unsafe active register overlap | Intermediate experimental SHA_TEXT writes during BUSY could silently lose candidates | Disabled from public path; sequential documented peripheral contract retained | SOURCE VERIFIED; not advertised as a speedup |
| TLS/SHA contention | Whole HTTPS request pushed HW miner to ~37 kH/s software-only work | Software TLS SHA ALT and bounded post-lock CPU windows keep HW worker active | PHYSICALLY TESTED: multiple HTTP 200 refreshes, zero HW→SW fallback; actual work measured |
| PoolStats fallback | Stock display could query Public Pool for an unrelated selected pool | Explicit registry/provider; unknown host makes no stats request | HOST TESTED: provider selection |
| Helios API 400 | Old query route returned “Invalid address”; dashboard became N/A | Deployed path route and bounded first-user parsing | PHYSICALLY TESTED: repeated HTTP 200 with numeric metrics |
| TLS chain and clock | Older bundle/clock handling could fail secure Helios HTTPS | Dedicated current root, hostname validation, valid-UTC gate and global SNTP | HOST TESTED certificate/clock policy; PHYSICALLY TESTED verified TLS |
| TLS stack/heap | Intermediate stats task could overflow stack or leave too little heap | Task stack sizing, reusable verified transport and borrowed display scratch | PHYSICALLY TESTED stack/heap logs; low free-heap remains a limitation |
| Oversize response/keep-alive | Intermediate short deadline closed a healthy large history response | Capture compact user object, bounded drain with corrected whole-response deadline | PHYSICALLY TESTED repeated parsed HTTP 200/keep-alive |
| Dashboard clipping and version overlap | CYD values/long development label extended outside visible bounds | Bounded metric coordinates and short public label in unused startup strip | PHYSICALLY TESTED visual user confirmation |
| Stratum partial write | Stock transmit routine reported success without checking bytes written | Compare written bytes with complete payload length | SOURCE VERIFIED; live accepted shares observed |

## Performance development and physical evidence

The approximately **340–345 kH/s** stock baseline is a physical observation
on the same board, not a host-derived SW/HW decomposition. Safe hardware
address reuse, protected polling, verified candidate/range handling and
TLS coexistence were retained only after correctness checks. Compiler
`-Os`, per-function/per-TU `-O2` and `-O3`, worker affinity/priority,
two-software-worker, compact ring-schedule and hardware midstate approaches
were explored locally. The compact 16-word SW schedule passed five million
cases but was slower and spilled more on Xtensa; it was rejected. Active
`SHA_TEXT` register-fill overlap was rejected because documented behavior
could not exclude silent false negatives. Non-stalled DPORT alternatives
produced physical SHA mismatches. Higher experimental counter values from
incorrect variants are **not valid mining performance**.

PHYSICALLY TESTED: the retained source state completed 195,645,712 hashes
in 448.72 seconds after warm-up (**436.01 kH/s**) with 227 accepted /
0 rejected shares, zero candidate-validation failures and six successful
Helios refreshes. The exact public application was then browser-installed
and read back byte-for-byte: 78,106,120 hashes in 178.633 seconds
(**437.24 kH/s**), 111 accepted / 0 rejected shares, three verified Helios
HTTP 200 refreshes and zero HW→SW fallback. The browser test's mean
displayed rate was 436.85 kH/s. These are single-board windows, not
multi-day guarantees or an assertion of 700 kH/s or 1 MH/s.

HOST TESTED: 5,000,000 deterministic optimized/reference SHA-256d
comparisons, zero mismatches; known Bitcoin headers, target boundaries,
candidate corruption, generation and nonce-range tests. PHYSICALLY TESTED:
1,800,099 full digest comparisons plus 1,179,648 unforced early-filter
comparisons (2,979,747 total), zero mismatch. BUILD VERIFIED: the clean
ESP32_2432S028_2USB application used 56,380 bytes static RAM, 2,154,693
bytes reported program flash, and 110,863 bytes IRAM text plus 1,027 bytes
vectors. The physical release-byte test, build comparison and limitations
are detailed in [hardware validation](release/HARDWARE_VALIDATION.md),
[binary comparison](release/BINARY_COMPARISON.md), [Helios diagnosis](HELIOS_REFRESH_FIX.md),
and [SHA hardware fix](SHA_HARDWARE_FIX.md).

KNOWN LIMITATION: TLS-time free heap has approached a few kilobytes and
the corrected overnight Helios failure has not been re-observed over a
full nine-hour post-fix run. Other board variants, network conditions,
very long uptime and external CA/API changes require their own validation.

## Diff coverage and file-by-file appendix

The appendix below is generated from the exact official-tag-to-public-tree
name/status inventory. Every changed file is listed; whole-file additions
are treated as one file-level diff unit. For multi-hunk mining, display,
PoolStats, test and build files, the named functions and architectural
sections above explain the meaningful changes. The final hunk-location
inventory following the appendix records all modified-file hunks, including
formatting, punctuation, comments, labels, coordinates and version-only
edits; these are non-functional unless a section above states otherwise.
Inherited tag-to-import differences are marked explicitly. Source-only
test and diagnostic material is not a production runtime requirement.

<!-- FILE_APPENDIX_START -->
| File | Status | What changed | Why | User impact | Category |
|---|---|---|---|---|---|
| `.gitignore` | modified | Ignore generated outputs and legacy prebuilt bin/ directory. | Keep private/development artifacts out of public history. | none | release hygiene |
| `.vscode/extensions.json` | removed (also differs in the pre-FX1 import) | Inherited import omitted editor recommendations. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `CHANGELOG.md` | added | Human-readable FX1 RC1 feature, fix, performance and limitation history. | Summarize changes without hiding rejected/untested claims. | visible | docs |
| `LICENSES/Apache-2.0.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `LICENSES/certifi-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `PERFORMANCE_RELEASE_NOTES.md` | added | Mark earlier development/release notes as historical. | Prevent obsolete candidate instructions being mistaken for RC1. | none | docs |
| `README.md` | modified (also differs in the pre-FX1 import) | Concise FX1 overview plus preserved upstream guidance. | Give owners accurate tested-board and install information. | visible | docs |
| `RELEASE_NOTES.md` | added | Mark earlier development/release notes as historical. | Prevent obsolete candidate instructions being mistaken for RC1. | none | docs |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDMonoWinTT/LICENSE.TXT` | modified (also differs in the pre-FX1 import) | Inherited font license/README text normalization or formatting; original attribution retained. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDMonoWinTT/README.TXT` | modified (also differs in the pre-FX1 import) | Inherited font license/README text normalization or formatting; original attribution retained. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDWinTT/LICENSE.TXT` | modified (also differs in the pre-FX1 import) | Inherited font license/README text normalization or formatting; original attribution retained. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDWinTT/README.TXT` | modified (also differs in the pre-FX1 import) | Inherited font license/README text normalization or formatting; original attribution retained. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `SLS/assets/Fonts/modern_lcd_7/readme.txt` | modified (also differs in the pre-FX1 import) | Inherited font license/README text normalization or formatting; original attribution retained. | Reason not conclusively recoverable from repository history. | none | import/cosmetic |
| `THIRD_PARTY_NOTICES.md` | added | ESP-IDF, mbedTLS, certifi and upstream notices. | Preserve license provenance. | none | license |
| `auto_firmware_version.py` | modified | Version/merge build helper adjustment. | Produce traceable image components and deterministic release packaging. | internal | build |
| `bin/bin DUO/DUO_A/0x0000_bootloader.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_A/0x10000_firmware.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_A/0x8000_partitions.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_A/0xe000_boot_app0.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_B/0x0000_bootloader.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_B/0x10000_firmware.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_B/0x8000_partitions.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin DUO/DUO_B/0xe000_boot_app0.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin ESP32-devKit- no pass/0x10000_firmware.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin ESP32-devKit- no pass/0x1000_bootloader.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin ESP32-devKit- no pass/0x8000_partitions.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin ESP32-devKit- no pass/0xe000_boot_app0.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin LYLYGO TDisplay S3 - no pass/0x0000_bootloader.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin LYLYGO TDisplay S3 - no pass/0x10000_firmware.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin LYLYGO TDisplay S3 - no pass/0x8000_partitions.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/bin LYLYGO TDisplay S3 - no pass/0xe000_boot_app0.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32/multi_download.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32/security.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32/spi_download.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32/utility.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/security.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/spi_download.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/utility.conf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/0x0000_NerdMinerV2.dio.bootloader.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/0x0000_NerdMinerV2.ino.bootloader.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/0x1000_bootloader.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/0x1000_bootloader_qio_80m.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/2.Seeder.ino.bootloader.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/_temp_by_dltool/downloadPanel1/bootloader_qio_80m.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/bin_tmp/downloadPanel1/0x10000_firmware.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/bin_tmp/downloadPanel1/0x1000_bootloader.bin_rep` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/bin_tmp/downloadPanel1/0x8000_partitions.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/dl_temp/bin_tmp/downloadPanel1/0xe000_boot_app0.bin` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/doc/Flash_Download_Tool__cn.pdf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/doc/Flash_Download_Tool__en.pdf` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/flash_download_tool_3.9.3.exe` | removed | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/logs/0CB815F2D130.txt` | removed (also differs in the pre-FX1 import) | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `bin/flash_download_tool_3.9.3/logs/68B6B3233388.txt` | removed (also differs in the pre-FX1 import) | Removed inherited prebuilt images/Windows flasher and its local-state files. | Avoid stale binaries, tool-local paths and third-party executable in public FX1 history. | none | release hygiene |
| `data/cert/README.md` | added | CA bundle or documented GTS/ISRG Helios root material. | Verify HTTPS chains without insecure fallback. | internal | TLS/certificate |
| `data/cert/gts_root_r4.pem` | added | CA bundle or documented GTS/ISRG Helios root material. | Verify HTTPS chains without insecure fallback. | internal | TLS/certificate |
| `data/cert/isrg_root_x2.pem` | added | CA bundle or documented GTS/ISRG Helios root material. | Verify HTTPS chains without insecure fallback. | internal | TLS/certificate |
| `data/cert/x509_crt_bundle.bin` | added | CA bundle or documented GTS/ISRG Helios root material. | Verify HTTPS chains without insecure fallback. | internal | TLS/certificate |
| `docs/.nojekyll` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/BUILD_AND_TEST.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/DETAILED_CHANGES.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/HARDWARE_VALIDATION.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/HELIOS_REFRESH_FIX.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/MULTIPOOL_ARCHITECTURE.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/PERF9_RESULTS.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/PERFORMANCE_TESTING.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/SECURITY_PRIVACY.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/SHA_HARDWARE_FIX.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/UI_RENDER_MOCKS.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/VALIDATION.md` | added | Technical documentation, release/site metadata or historical test record. | Publish evidence, architecture and limitations without changing firmware. | none | docs |
| `docs/favicon.svg` | added | Static panel illustration or site icon. | Provide visual context without changing firmware graphics. | visible | web/assets |
| `docs/images/pool-panel-heliospool.png` | added | Static panel illustration or site icon. | Provide visual context without changing firmware graphics. | visible | web/assets |
| `docs/images/pool-panel-public-pool.png` | added | Static panel illustration or site icon. | Provide visual context without changing firmware graphics. | visible | web/assets |
| `docs/images/pool-panel-stale.png` | added | Static panel illustration or site icon. | Provide visual context without changing firmware graphics. | visible | web/assets |
| `docs/images/pool-panel-unsupported.png` | added | Static panel illustration or site icon. | Provide visual context without changing firmware graphics. | visible | web/assets |
| `docs/index.html` | added | Static RC1 web flasher landing page and explicit update/factory choices. | Make the verified app-only path prominent and configuration-safe. | visible | web/release |
| `docs/manifest-factory.json` | added | Factory-image manifest at 0x0000. | Allow intentional clean installation with clear warning. | visible | web/release |
| `docs/manifest-update.json` | added | Single-part app manifest at 0x10000. | Flash exact tested bytes without touching configuration partitions. | visible | web/release |
| `docs/release/.gitattributes` | added | Bundled public release notes, comparison or attribution. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/BINARY_COMPARISON.md` | added | Bundled public release notes, comparison or attribution. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/BUILD_INFO.txt` | added | Build provenance and resource measurements. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/CHANGELOG.md` | added | Bundled public release notes, comparison or attribution. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/FLASHING.md` | added | Update/factory/rollback instructions. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/HARDWARE_VALIDATION.md` | added | Physical and host validation evidence. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/LICENSE` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/Adafruit-GFX-license.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/Apache-2.0.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/ArduinoJson-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/Boost-1.0.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/FreeType-FTL.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/NTPClient-MIT.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/OneButton-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/OpenFontRender-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/TFT_eSPI-license.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/WiFiManager-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/certifi-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/LICENSES/mbedtls-LICENSE.txt` | added | Preserved/copied third-party or upstream license text. | Maintain legal attribution for bundled source and trust material. | none | license |
| `docs/release/NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-application.bin` | added | Frozen tested application or merged factory image. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-factory.bin` | added | Frozen tested application or merged factory image. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/NerdMinerV2-V1.8.3-FX1-source.zip` | added | Auditable source archive. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/SHA256SUMS.txt` | added | Release artifact checksums. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `docs/release/THIRD_PARTY_NOTICES.md` | added | Bundled public release notes, comparison or attribution. | Ship a verified and inspectable unofficial RC1 package. | visible | release artifact |
| `lib/TFT_eSPI/User_Setup_Select.h` | modified (also differs in the pre-FX1 import) | Inherited display setup selection/board-specific setup changes. | Reason not conclusively recoverable from repository history; not the tested CYD target. | internal | import/board support |
| `lib/TFT_eSPI/User_Setups/Setup215_M5_Cardputer_Adv.h` | added (also differs in the pre-FX1 import) | Inherited display setup selection/board-specific setup changes. | Reason not conclusively recoverable from repository history; not the tested CYD target. | internal | import/board support |
| `lib/TFT_eSPI/User_Setups/Setup_Spotpear_ST7735.h` | modified (also differs in the pre-FX1 import) | Inherited display setup selection/board-specific setup changes. | Reason not conclusively recoverable from repository history; not the tested CYD target. | internal | import/board support |
| `platformio.ini` | modified (also differs in the pre-FX1 import) | Inherited board environments plus FX1 target flags, TLS sources, certificate embedding and build checks. | Build tested classic ESP32 with compatible verified TLS and mining code. | internal | import + build |
| `post_build_merge.py` | modified | Version/merge build helper adjustment. | Produce traceable image components and deterministic release packaging. | internal | build |
| `src/NerdMinerV2.ino.cpp` | modified (also differs in the pre-FX1 import) | setup(): inherited power-latch comment plus FX1 display/PoolStats initialization and valid-time startup. | Keep board initialization and stats task separated from mining. | internal | import + pool/network |
| `src/ShaTests/nerdSHA256plus.cpp` | modified | Optimized SHA bake/filter or hardware-test access correction. | Maintain exact mining work and hardware comparison behavior. | internal | SHA/correctness |
| `src/ShaTests/nerdSHA_HWTest.cpp` | modified | Optimized SHA bake/filter or hardware-test access correction. | Maintain exact mining work and hardware comparison behavior. | internal | SHA/correctness |
| `src/crypto/ClassicEsp32ShaAccess.h` | added | Contract-safe DPORT/SHA polling and MEMW ordering. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/crypto/ClassicShaDiagnostics.h` | added | Development-only physical SHA comparison helpers. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/crypto/MiningRangePolicy.h` | added | Nonce range and submit-format policy. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/crypto/ReferenceSha256.cpp` | added | Independent reference Bitcoin SHA-256d. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/crypto/ReferenceSha256.h` | added | Independent reference Bitcoin SHA-256d. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/crypto/ShaResourcePolicy.h` | added | Bounded SHA resource/CPU-window policy. | Enforce correct candidate/hash ownership without unsafe peripheral overlap. | internal | mining/safety |
| `src/drivers/devices/M5Stick-C-Plus2.h` | modified (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/drivers/devices/device.h` | modified (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/drivers/devices/lilygoT3V1.h` | added (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/drivers/devices/m5CardputerAdv.h` | added (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/drivers/devices/spotpearKeychain.h` | modified (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/drivers/displays/display.cpp` | modified (also differs in the pre-FX1 import) | Display scratch borrowing/restoration and rendering guard. | Free cold-TLS heap while retaining complete frames and backlight behavior. | visible | display/memory |
| `src/drivers/displays/display.h` | modified | Display scratch borrowing/restoration and rendering guard. | Free cold-TLS heap while retaining complete frames and backlight behavior. | visible | display/memory |
| `src/drivers/displays/displayDriver.h` | modified (also differs in the pre-FX1 import) | Inherited or FX1-compatible board display adapter/pool panel adjustments. | Keep secondary display paths buildable; only CYD physically validated. | visible | import/display |
| `src/drivers/displays/esp23_2432s028r.cpp` | modified | CYD pool panel, clipping, scratch lifecycle and V1.8.3-FX1 startup label coordinates. | Show truthful Helios metrics and avoid overlap without redesign. | visible | display/UI |
| `src/drivers/displays/sp_kcDisplayDriver.cpp` | modified (also differs in the pre-FX1 import) | Inherited or FX1-compatible board display adapter/pool panel adjustments. | Keep secondary display paths buildable; only CYD physically validated. | visible | import/display |
| `src/drivers/displays/ssd1306DisplayDriver.cpp` | added (also differs in the pre-FX1 import) | Inherited or FX1-compatible board display adapter/pool panel adjustments. | Keep secondary display paths buildable; only CYD physically validated. | visible | import/display |
| `src/drivers/displays/t_hmiDisplayDriver.cpp` | modified | Inherited or FX1-compatible board display adapter/pool panel adjustments. | Keep secondary display paths buildable; only CYD physically validated. | visible | import/display |
| `src/drivers/displays/wt32DisplayDriver.cpp` | modified | Inherited or FX1-compatible board display adapter/pool panel adjustments. | Keep secondary display paths buildable; only CYD physically validated. | visible | import/display |
| `src/drivers/storage/storage.h` | modified (also differs in the pre-FX1 import) | Inherited default pool-port setting. | Reason not conclusively recoverable from repository history. | visible | import/config |
| `src/i2c_master.cpp` | modified (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/i2c_master.h` | modified (also differs in the pre-FX1 import) | Inherited additional board or I2C support relative to official tag. | Reason not conclusively recoverable from repository history; not FX1 CYD mining path. | internal | import/board support |
| `src/mining.cpp` | modified | JobPush, MiningJobStop, runStratumWorker, minerWorkerSw/Hw, SHA helpers, counters and stat restore/save: snapshots, validation, ranges, exact target/filter, DPORT-safe SHA. | Prevent invalid/stale/duplicate mining work and improve measured useful throughput. | visible | mining/correctness/performance |
| `src/mining.h` | modified | Updated mining interfaces and long-running counters. | Match synchronized completed-hash and job state. | internal | mining |
| `src/monitor.cpp` | modified | Replace blocking Public Pool fetch with PoolStats snapshot; 64-bit display counter type. | Honest provider selection and nonblocking dashboard rendering. | visible | PoolStats/display |
| `src/monitor.h` | modified | Replace blocking Public Pool fetch with PoolStats snapshot; 64-bit display counter type. | Honest provider selection and nonblocking dashboard rendering. | visible | PoolStats/display |
| `src/poolstats/HeliosUserCapture.h` | added | Bounded streaming capture of the current Helios user object. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/HeliosUserPrefix.h` | added | Bounded streaming capture of the current Helios user object. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolRegistry.cpp` | added | Pool hostname normalization, display naming and wallet extraction. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolRegistry.h` | added | Pool hostname normalization, display naming and wallet extraction. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsParsers.cpp` | added | Public Pool/Helios JSON and time/metric parsing. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsParsers.h` | added | Public Pool/Helios JSON and time/metric parsing. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsPolicy.cpp` | added | Availability, cache, retry and HTTP-status policy. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsPolicy.h` | added | Availability, cache, retry and HTTP-status policy. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsProvider.cpp` | added | Provider interfaces, endpoint selection and metric snapshot types. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsProvider.h` | added | Provider interfaces, endpoint selection and metric snapshot types. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsService.cpp` | added | Low-priority refresh task and synchronized dashboard snapshot. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsService.h` | added | Low-priority refresh task and synchronized dashboard snapshot. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/PoolStatsTypes.h` | added | Provider interfaces, endpoint selection and metric snapshot types. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/StatsTlsClient.cpp` | added | Verified TLS transport with bounded SHA/CPU coexistence. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/poolstats/StatsTlsClient.h` | added | Verified TLS transport with bounded SHA/CPU coexistence. | Keep statistics correct, bounded and independent from Stratum mining. | visible | pool/provider/TLS |
| `src/stratum.cpp` | modified | tx_mining_submit fixed-width nonce and complete-write result; trailing newline cleanup. | Preserve correct wire serialization and detect failed sends. | internal | Stratum/correctness |
| `src/utils.cpp` | modified | Target/endian and Merkle-root helper corrections. | Exact Bitcoin comparison and bounded Merkle buffer writes. | internal | correctness/memory |
| `src/utils.h` | modified | Target/endian and Merkle-root helper corrections. | Exact Bitcoin comparison and bounded Merkle buffer writes. | internal | correctness/memory |
| `src/version.h` | modified | Public startup label and separate internal build identifier. | Short visible label without losing build provenance. | visible | version/UI |
| `test/README.md` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/fixtures/helios_snapshot.json` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/fixtures/malformed.json` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/fixtures/missing_fields.json` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/fixtures/public_pool.json` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/native_mining_validation.cpp` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/native_poolstats.cpp` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/native_stubs/Arduino.h` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/native_stubs/esp_log.h` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/native_stubs/esp_timer.h` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `test/verify_helios_root.py` | added | Native fixtures, stubs and mining/PoolStats/certificate tests. | Catch SHA, candidate, target, range, parsing and TLS-regression errors. | internal | tests |
| `tools/package_release.ps1` | added | Build, package, native-test, renderer or Xtensa-codegen utility. | Make release/test evidence repeatable without modifying runtime mining. | internal | tooling |
| `tools/render_pool_panel.py` | added | Build, package, native-test, renderer or Xtensa-codegen utility. | Make release/test evidence repeatable without modifying runtime mining. | internal | tooling |
| `tools/run_native_tests.ps1` | added | Build, package, native-test, renderer or Xtensa-codegen utility. | Make release/test evidence repeatable without modifying runtime mining. | internal | tooling |
| `tools/validate_classic_sha_codegen.py` | added | Build, package, native-test, renderer or Xtensa-codegen utility. | Make release/test evidence repeatable without modifying runtime mining. | internal | tooling |
| `vendor/mbedtls-tls/LICENSE` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/ORIGIN.md` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/common.h` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/constant_time_internal.h` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/ssl_cli.c` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/ssl_msg.c` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/ssl_srv.c` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
| `vendor/mbedtls-tls/ssl_tls.c` | added | Pinned, attributed mbedTLS 2.28.4 TLS runtime source or license. | Support bounded output record sizing with SDK ABI match. | internal | TLS/vendor |
<!-- FILE_APPENDIX_END -->

## Trivial/cosmetic and hunk-location inventory

The following categories cover small changes that do not warrant separate
runtime claims: license/readme line endings and wording in inherited font
assets; whitespace and newline normalization in imported board support and
`parse_extract_id`; comment/diagnostic wording; CYD text coordinates and
version-only label; README, notices, checksum and build-info metadata; and
visual mock images. No category here implies a new hardware feature.
Line locations below are the zero-context Git diff hunk headers; they are an
index into the exact stock-to-FX1 patch, not separate performance claims.

<!-- HUNK_INVENTORY_START -->
| Changed file | Zero-context hunk locations (and Git function context where supplied) | Coverage |
|---|---|---|
| `.gitignore` | `-8 +8,2 NerdMinerLog.txt`<br>`-9,0 +11,2 logs` | file appendix |
| `.vscode/extensions.json` | `-1,10 +0,0 ` | file appendix |
| `CHANGELOG.md` | `-0,0 +1,92 ` | documentation/release artifact |
| `LICENSES/Apache-2.0.txt` | `-0,0 +1,201 ` | file appendix |
| `LICENSES/certifi-LICENSE.txt` | `-0,0 +1,20 ` | file appendix |
| `PERFORMANCE_RELEASE_NOTES.md` | `-0,0 +1,138 ` | documentation/release artifact |
| `README.md` | `-1 +1,53 `<br>`-124 +176 Note: when BTC address of your selected walle`<br>`-126,5 +178,5 Note: when BTC address of your selected walle`<br>`-132,3 +184,3 Note: when BTC address of your selected walle`<br>`-148 +200 Recommended low difficulty share pools:` | documentation/release artifact |
| `RELEASE_NOTES.md` | `-0,0 +1,30 ` | documentation/release artifact |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDMonoWinTT/LICENSE.TXT` | `-1,74 +1,74 ` | inherited formatting/asset |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDMonoWinTT/README.TXT` | `-1,44 +1,44 ` | inherited formatting/asset |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDWinTT/LICENSE.TXT` | `-1,74 +1,74 ` | inherited formatting/asset |
| `SLS/assets/Fonts/lcd_lcd_mono/LCDWinTT/README.TXT` | `-1,63 +1,63 ` | inherited formatting/asset |
| `SLS/assets/Fonts/modern_lcd_7/readme.txt` | `-1,42 +1,42 ` | inherited formatting/asset |
| `THIRD_PARTY_NOTICES.md` | `-0,0 +1,36 ` | documentation/release artifact |
| `auto_firmware_version.py` | `-1,0 +2,3 import subprocess`<br>`-6,5 +9,6 def get_firmware_specifier_build_flag():`<br>`-17,0 +22,27 env.Append(` | build/release tooling |
| `bin/flash_download_tool_3.9.3/configure/esp32/multi_download.conf` | `-1,121 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32/security.conf` | `-1,18 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32/spi_download.conf` | `-1,94 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32/utility.conf` | `-1,11 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/security.conf` | `-1,18 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/spi_download.conf` | `-1,94 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/configure/esp32s3/utility.conf` | `-1,10 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/logs/0CB815F2D130.txt` | `-1,32 +0,0 ` | legacy-bin removal |
| `bin/flash_download_tool_3.9.3/logs/68B6B3233388.txt` | `-1,390 +0,0 ` | legacy-bin removal |
| `data/cert/README.md` | `-0,0 +1,47 ` | Pool providers, API and TLS |
| `data/cert/gts_root_r4.pem` | `-0,0 +1,13 ` | Pool providers, API and TLS |
| `data/cert/isrg_root_x2.pem` | `-0,0 +1,14 ` | Pool providers, API and TLS |
| `docs/.nojekyll` | `-0,0 +1 ` | documentation/release artifact |
| `docs/BUILD_AND_TEST.md` | `-0,0 +1,64 ` | documentation/release artifact |
| `docs/DETAILED_CHANGES.md` | `-0,0 +1,590 ` | documentation/release artifact |
| `docs/HARDWARE_VALIDATION.md` | `-0,0 +1,75 ` | documentation/release artifact |
| `docs/HELIOS_REFRESH_FIX.md` | `-0,0 +1,92 ` | documentation/release artifact |
| `docs/MULTIPOOL_ARCHITECTURE.md` | `-0,0 +1,95 ` | documentation/release artifact |
| `docs/PERF9_RESULTS.md` | `-0,0 +1,205 ` | documentation/release artifact |
| `docs/PERFORMANCE_TESTING.md` | `-0,0 +1,171 ` | documentation/release artifact |
| `docs/SECURITY_PRIVACY.md` | `-0,0 +1,23 ` | documentation/release artifact |
| `docs/SHA_HARDWARE_FIX.md` | `-0,0 +1,57 ` | documentation/release artifact |
| `docs/UI_RENDER_MOCKS.md` | `-0,0 +1,31 ` | documentation/release artifact |
| `docs/VALIDATION.md` | `-0,0 +1,60 ` | documentation/release artifact |
| `docs/favicon.svg` | `-0,0 +1,4 ` | documentation/release artifact |
| `docs/index.html` | `-0,0 +1,132 ` | documentation/release artifact |
| `docs/manifest-factory.json` | `-0,0 +1,17 ` | documentation/release artifact |
| `docs/manifest-update.json` | `-0,0 +1,17 ` | documentation/release artifact |
| `docs/release/.gitattributes` | `-0,0 +1,4 ` | documentation/release artifact |
| `docs/release/BINARY_COMPARISON.md` | `-0,0 +1,10 ` | documentation/release artifact |
| `docs/release/BUILD_INFO.txt` | `-0,0 +1,22 ` | documentation/release artifact |
| `docs/release/CHANGELOG.md` | `-0,0 +1,92 ` | documentation/release artifact |
| `docs/release/FLASHING.md` | `-0,0 +1,29 ` | documentation/release artifact |
| `docs/release/HARDWARE_VALIDATION.md` | `-0,0 +1,75 ` | documentation/release artifact |
| `docs/release/LICENSE` | `-0,0 +1,24 ` | documentation/release artifact |
| `docs/release/LICENSES/Adafruit-GFX-license.txt` | `-0,0 +1,34 ` | documentation/release artifact |
| `docs/release/LICENSES/Apache-2.0.txt` | `-0,0 +1,201 ` | documentation/release artifact |
| `docs/release/LICENSES/ArduinoJson-LICENSE.txt` | `-0,0 +1,10 ` | documentation/release artifact |
| `docs/release/LICENSES/Boost-1.0.txt` | `-0,0 +1,23 ` | documentation/release artifact |
| `docs/release/LICENSES/FreeType-FTL.txt` | `-0,0 +1,169 ` | documentation/release artifact |
| `docs/release/LICENSES/NTPClient-MIT.txt` | `-0,0 +1,18 ` | documentation/release artifact |
| `docs/release/LICENSES/OneButton-LICENSE.txt` | `-0,0 +1,15 ` | documentation/release artifact |
| `docs/release/LICENSES/OpenFontRender-LICENSE.txt` | `-0,0 +1,7 ` | documentation/release artifact |
| `docs/release/LICENSES/TFT_eSPI-license.txt` | `-0,0 +1,135 ` | documentation/release artifact |
| `docs/release/LICENSES/WiFiManager-LICENSE.txt` | `-0,0 +1,22 ` | documentation/release artifact |
| `docs/release/LICENSES/certifi-LICENSE.txt` | `-0,0 +1,20 ` | documentation/release artifact |
| `docs/release/LICENSES/mbedtls-LICENSE.txt` | `-0,0 +1,202 ` | documentation/release artifact |
| `docs/release/SHA256SUMS.txt` | `-0,0 +1,22 ` | documentation/release artifact |
| `docs/release/THIRD_PARTY_NOTICES.md` | `-0,0 +1,57 ` | documentation/release artifact |
| `lib/TFT_eSPI/User_Setup_Select.h` | `-151,0 +152,3 ` | inherited board support |
| `lib/TFT_eSPI/User_Setups/Setup215_M5_Cardputer_Adv.h` | `-0,0 +1,31 ` | inherited board support |
| `lib/TFT_eSPI/User_Setups/Setup_Spotpear_ST7735.h` | `-1,83 +1,83 ` | inherited board support |
| `platformio.ini` | `-10,0 +11,5 `<br>`-14 +19 globallib_dir = lib`<br>`-17,0 +23,2 default_envs =  NerdminerV2, NerdminerV2-T-HM`<br>`-51 +57,0 lib_deps =`<br>`-1112,0 +1119,26 lib_ignore =`<br>`-1144 +1176 extra_scripts =`<br>`-1151 +1183 board_build.partitions = huge_app.csv`<br>`-1156 +1188 build_flags =`<br>`-1162 +1194 lib_deps =`<br>`-1168,0 +1201 lib_ignore =`<br>`-1170 +1203,29 lib_ignore =` | build/release tooling |
| `post_build_merge.py` | `-14,0 +15 import subprocess`<br>`-92 +93 def get_firmware_version():`<br>`-94,8 +95,5 def get_firmware_version():`<br>`-120,0 +119,10 def create_merged_firmware(source, target, en`<br>`-138,3 +146,3 def create_merged_firmware(source, target, en`<br>`-167,0 +176,6 def create_merged_firmware(source, target, en`<br>`-171,17 +185,17 def create_merged_firmware(source, target, en`<br>`-199,0 +214 def create_merged_firmware(source, target, en`<br>`-202 +217 def create_merged_firmware(source, target, en` | build/release tooling |
| `src/NerdMinerV2.ino.cpp` | `-62,0 +63 void setup()`<br>`-79,0 +81,4 void setup()`<br>`-163 +168 void setup()` | file appendix |
| `src/ShaTests/nerdSHA256plus.cpp` | `-457 +457 IRAM_ATTR bool nerd_sha256d(nerdSHA256_contex`<br>`-485,0 +486,4 IRAM_ATTR void nerd_sha256_bake(const uint32_`<br>`-530,2 +534,4 IRAM_ATTR bool nerd_sha256d_baked(const uint3` | Mining and Bitcoin correctness / SHA hardware |
| `src/ShaTests/nerdSHA_HWTest.cpp` | `-244 +244 IRAM_ATTR void HwShaTest()`<br>`-531 +531 IRAM_ATTR void HwShaTest()` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/ClassicEsp32ShaAccess.h` | `-0,0 +1,113 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/ClassicShaDiagnostics.h` | `-0,0 +1,134 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/MiningRangePolicy.h` | `-0,0 +1,31 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/ReferenceSha256.cpp` | `-0,0 +1,160 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/ReferenceSha256.h` | `-0,0 +1,29 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/crypto/ShaResourcePolicy.h` | `-0,0 +1,78 ` | Mining and Bitcoin correctness / SHA hardware |
| `src/drivers/devices/M5Stick-C-Plus2.h` | `-6,0 +7 ` | inherited board support |
| `src/drivers/devices/device.h` | `-25,0 +26,2 `<br>`-55,0 +58,2 ` | inherited board support |
| `src/drivers/devices/lilygoT3V1.h` | `-0,0 +1,12 ` | inherited board support |
| `src/drivers/devices/m5CardputerAdv.h` | `-0,0 +1,13 ` | inherited board support |
| `src/drivers/devices/spotpearKeychain.h` | `-1,27 +1,27 ` | inherited board support |
| `src/drivers/displays/display.cpp` | `-1,0 +2,13 `<br>`-66,0 +80,4 DisplayDriver *currentDisplayDriver = &sp_kcD`<br>`-70,0 +88,3 void initDisplay()`<br>`-73,0 +94,19 void initDisplay()`<br>`-112,0 +152,18 void drawCurrentScreen(unsigned long mElapsed`<br>`-113,0 +171,3 void drawCurrentScreen(unsigned long mElapsed` | Display, memory, build and release |
| `src/drivers/displays/display.h` | `-17,0 +18,4 void doLedStuff(unsigned long frame);` | Display, memory, build and release |
| `src/drivers/displays/displayDriver.h` | `-45,0 +46 extern DisplayDriver sp_kcDisplayDriver;` | Display, memory, build and release |
| `src/drivers/displays/esp23_2432s028r.cpp` | `-8 +7,0 `<br>`-14,0 +14 `<br>`-17,0 +18 `<br>`-26 +27,30 TFT_eSPI tft = TFT_eSPI();                  /`<br>`-89,0 +120,4 void esp32_2432S028R_Init(void)`<br>`-104,3 +137,0 void esp32_2432S028R_Init(void)`<br>`-129,0 +161,14 bool bottomScreenBlue = true;`<br>`-152,2 +196,0 bool createBackgroundSprite(int16_t wdt, int1`<br>`-155,52 +198,42 void printPoolData(){`<br>`-207,0 +241,23 void printPoolData(){`<br>`-512,0 +569,5 void esp32_2432S028R_LoadingScreen(void)`<br>`-514 +574,0 void esp32_2432S028R_LoadingScreen(void)` | Display, memory, build and release |
| `src/drivers/displays/sp_kcDisplayDriver.cpp` | `-1,181 +1,181 ` | Display, memory, build and release |
| `src/drivers/displays/ssd1306DisplayDriver.cpp` | `-0,0 +1,132 ` | Display, memory, build and release |
| `src/drivers/displays/t_hmiDisplayDriver.cpp` | `-13,0 +14 `<br>`-19,0 +21 `<br>`-95,3 +96,0 void t_hmiDisplay_Init(void)`<br>`-112,0 +112,12 void t_hmiDisplay_AlternateRotation(void)`<br>`-119 +130,26 void printPoolData()`<br>`-121,7 +157,5 void printPoolData()` | Display, memory, build and release |
| `src/drivers/displays/wt32DisplayDriver.cpp` | `-13,0 +14 `<br>`-337,3 +338,12 void wt32Display_NoScreen(unsigned long mElap` | Display, memory, build and release |
| `src/drivers/storage/storage.h` | `-18 +18 ` | file appendix |
| `src/i2c_master.cpp` | `-1,184 +1,184 ` | inherited board support |
| `src/i2c_master.h` | `-1,8 +1,8 ` | inherited board support |
| `src/mining.cpp` | `-4,0 +5 `<br>`-16,0 +18 `<br>`-20,0 +23,3 `<br>`-37,0 +43,7 `<br>`-48 +60 uint32_t Mhashes = 0;`<br>`-72,0 +85,3 unsigned long mLastTXtoPool = millis();`<br>`-110,2 +125,2 bool checkPoolInactivity(unsigned int keepAli`<br>`-131 +146 bool checkPoolInactivity(unsigned int keepAli`<br>`-144 +159 struct JobRequest`<br>`-150 +165,3 struct JobRequest`<br>`-155 +172 struct JobResult`<br>`-159,0 +177,3 struct JobResult`<br>`-168 +188,10 std::list<std::shared_ptr<JobResult>> s_job_r`<br>`-170,2 +199,13 static volatile uint8_t s_working_current_job`<br>`-174 +214 static void JobPush(std::list<std::shared_ptr`<br>`-180,0 +221,2 static void JobPush(std::list<std::shared_ptr`<br>`-183,0 +226,56 static void JobPush(std::list<std::shared_ptr`<br>`-194,0 +293,3 static void MiningJobStop(uint32_t &job_pool,`<br>`-201 +301,0 static void MiningJobStop(uint32_t &job_pool,`<br>`-249,0 +350 void runStratumWorker(void *name) {`<br>`-252,0 +354,7 void runStratumWorker(void *name) {`<br>`-323,7 +430,0 void runStratumWorker(void *name) {`<br>`-339,0 +441 void runStratumWorker(void *name) {`<br>`-341,0 +444 void runStratumWorker(void *name) {`<br>`-350 +452,0 void runStratumWorker(void *name) {`<br>`-355,4 +456,0 void runStratumWorker(void *name) {`<br>`-359,0 +458 void runStratumWorker(void *name) {`<br>`-401 +500,4 void runStratumWorker(void *name) {`<br>`-410 +512,4 void runStratumWorker(void *name) {`<br>`-412 +517,4 void runStratumWorker(void *name) {`<br>`-425 +533 void runStratumWorker(void *name) {`<br>`-482 +590 void runStratumWorker(void *name) {`<br>`-489 +597 void runStratumWorker(void *name) {`<br>`-490,0 +599 void runStratumWorker(void *name) {`<br>`-492,0 +602,4 void runStratumWorker(void *name) {`<br>`-511,0 +625,4 void runStratumWorker(void *name) {`<br>`-515,2 +631,0 void runStratumWorker(void *name) {`<br>`-521 +636,4 void runStratumWorker(void *name) {`<br>`-534 +652,4 void runStratumWorker(void *name) {`<br>`-536 +657,4 void runStratumWorker(void *name) {`<br>`-552,2 +676,2 void runStratumWorker(void *name) {`<br>`-556 +680,33 void runStratumWorker(void *name) {`<br>`-558 +714,8 void runStratumWorker(void *name) {`<br>`-570,5 +733 void runStratumWorker(void *name) {`<br>`-596,0 +756 void minerWorkerSw(void * task_id)`<br>`-599,6 +758,0 void minerWorkerSw(void * task_id)`<br>`-615,0 +770 void minerWorkerSw(void * task_id)`<br>`-617 +772 void minerWorkerSw(void * task_id)`<br>`-619,2 +774,3 void minerWorkerSw(void * task_id)`<br>`-626 +782,2 void minerWorkerSw(void * task_id)`<br>`-629,0 +787 void minerWorkerSw(void * task_id)`<br>`-630,0 +789,4 void minerWorkerSw(void * task_id)`<br>`-634 +796,2 void minerWorkerSw(void * task_id)`<br>`-798 +961 void minerWorkerHw(void * task_id)`<br>`-802,0 +966 void minerWorkerHw(void * task_id)`<br>`-805,6 +968,0 void minerWorkerHw(void * task_id)`<br>`-821 +979 void minerWorkerHw(void * task_id)`<br>`-825 +983 void minerWorkerHw(void * task_id)`<br>`-835,2 +993 void minerWorkerHw(void * task_id)`<br>`-837,0 +995 void minerWorkerHw(void * task_id)`<br>`-870 +1028,2 void minerWorkerHw(void * task_id)`<br>`-872,2 +1030,0 void minerWorkerHw(void * task_id)`<br>`-875,0 +1033 void minerWorkerHw(void * task_id)`<br>`-877 +1035,4 void minerWorkerHw(void * task_id)`<br>`-882 +1043 void minerWorkerHw(void * task_id)`<br>`-905 +1066,2 void minerWorkerHw(void * task_id)`<br>`-907,5 +1069,4 static inline bool nerd_sha_ll_read_digest_sw`<br>`-913 +1074,4 static inline bool nerd_sha_ll_read_digest_sw`<br>`-923 +1087 static inline bool nerd_sha_ll_read_digest_sw`<br>`-940 +1104,3 static inline void nerd_sha_ll_read_digest(vo`<br>`-942,2 +1108 static inline void nerd_sha_hal_wait_idle()`<br>`-946 +1111 static inline void nerd_sha_hal_wait_idle()`<br>`-948,2 +1113,14 static inline void nerd_sha_ll_fill_text_bloc`<br>`-969 +1146,2 static inline void nerd_sha_ll_fill_text_bloc`<br>`-971 +1149 static inline void nerd_sha_ll_fill_text_bloc`<br>`-973,0 +1152,33 static inline void nerd_sha_ll_fill_text_bloc`<br>`-977 +1188 static inline void nerd_sha_ll_fill_text_bloc`<br>`-1007 +1218,2 static inline void nerd_sha_ll_fill_text_bloc`<br>`-1009 +1220,0 static inline void nerd_sha_ll_fill_text_bloc`<br>`-1031,0 +1243,304 static inline void nerd_sha_ll_fill_text_bloc`<br>`-1043,0 +1559 void minerWorkerHw(void * task_id)`<br>`-1046,6 +1561,0 void minerWorkerHw(void * task_id)`<br>`-1062 +1572 void minerWorkerHw(void * task_id)`<br>`-1064 +1573,0 void minerWorkerHw(void * task_id)`<br>`-1066 +1575 void minerWorkerHw(void * task_id)`<br>`-1069,2 +1578,7 void minerWorkerHw(void * task_id)`<br>`-1072,23 +1586 void minerWorkerHw(void * task_id)`<br>`-1096,18 +1588,12 void minerWorkerHw(void * task_id)`<br>`-1116 +1602,7 void minerWorkerHw(void * task_id)`<br>`-1145 +1637 void restoreStat() {`<br>`-1162,3 +1654,2 void restoreStat() {`<br>`-1177,0 +1669,3 void saveStat() {`<br>`-1179 +1673 void saveStat() {`<br>`-1187 +1681 void saveStat() {`<br>`-1200 +1694,5 void resetStat() {`<br>`-1219 +1717 void runMonitor(void *name)`<br>`-1234,2 +1732,3 void runMonitor(void *name)`<br>`-1236,0 +1736,4 void runMonitor(void *name)` | Mining and Bitcoin correctness / SHA hardware |
| `src/mining.h` | `-25,0 +26,3 void minerWorkerHw(void * task_id);`<br>`-39 +42 typedef struct{` | Mining and Bitcoin correctness / SHA hardware |
| `src/monitor.cpp` | `-12,0 +13 `<br>`-17 +18 extern uint32_t Mhashes;`<br>`-38 +38,0 pool_data pData;`<br>`-51,3 +51,3 void setup_monitor(void){`<br>`-205,2 +204,0 unsigned long initialTime = 0;`<br>`-340 +338 mining_data getMiningData(unsigned long mElap`<br>`-357 +355 clock_data getClockData(unsigned long mElapse`<br>`-385 +383 coin_data getCoinData(unsigned long mElapsed)`<br>`-408,31 +405,0 coin_data getCoinData(unsigned long mElapsed)`<br>`-440,74 +407 pool_data getPoolData(void){` | Pool providers, API and TLS |
| `src/monitor.h` | `-4,0 +5 `<br>`-33,5 +33,0 `<br>`-120,5 +116 typedef struct {`<br>`-134 +125,0 clock_data_t getClockData_t(unsigned long mEl` | Pool providers, API and TLS |
| `src/poolstats/HeliosUserCapture.h` | `-0,0 +1,39 ` | Pool providers, API and TLS |
| `src/poolstats/HeliosUserPrefix.h` | `-0,0 +1,16 ` | Pool providers, API and TLS |
| `src/poolstats/PoolRegistry.cpp` | `-0,0 +1,134 ` | Pool providers, API and TLS |
| `src/poolstats/PoolRegistry.h` | `-0,0 +1,11 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsParsers.cpp` | `-0,0 +1,201 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsParsers.h` | `-0,0 +1,16 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsPolicy.cpp` | `-0,0 +1,101 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsPolicy.h` | `-0,0 +1,30 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsProvider.cpp` | `-0,0 +1,362 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsProvider.h` | `-0,0 +1,20 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsService.cpp` | `-0,0 +1,231 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsService.h` | `-0,0 +1,11 ` | Pool providers, API and TLS |
| `src/poolstats/PoolStatsTypes.h` | `-0,0 +1,68 ` | Pool providers, API and TLS |
| `src/poolstats/StatsTlsClient.cpp` | `-0,0 +1,236 ` | Pool providers, API and TLS |
| `src/poolstats/StatsTlsClient.h` | `-0,0 +1,28 ` | Pool providers, API and TLS |
| `src/stratum.cpp` | `-11,0 +12 `<br>`-210,0 +212,2 bool tx_mining_submit(WiFiClient& client, min`<br>`-217 +220 bool tx_mining_submit(WiFiClient& client, min`<br>`-221 +224 bool tx_mining_submit(WiFiClient& client, min`<br>`-224 +227 bool tx_mining_submit(WiFiClient& client, min`<br>`-227 +230 bool tx_mining_submit(WiFiClient& client, min`<br>`-271 +274 unsigned long parse_extract_id(const String &` | Mining and Bitcoin correctness / SHA hardware |
| `src/utils.cpp` | `-5,0 +6 `<br>`-125,13 +126,2 bool isSha256Valid(const void* sha256)`<br>`-205,5 +195 miner_data calculateMiningData(mining_subscri`<br>`-324 +310 miner_data calculateMiningData(mining_subscri`<br>`-608 +593,0 uint32_t crc32_finish(uint32_t crc32)` | Mining and Bitcoin correctness / SHA hardware |
| `src/utils.h` | `-25 +25 miner_data calculateMiningData(mining_subscri`<br>`-34 +34 uint32_t crc32_finish(uint32_t crc32);` | Mining and Bitcoin correctness / SHA hardware |
| `src/version.h` | `-4 +4,2 ` | file appendix |
| `test/README.md` | `-0,0 +1,15 ` | test infrastructure |
| `test/fixtures/helios_snapshot.json` | `-0,0 +1,14 ` | test infrastructure |
| `test/fixtures/malformed.json` | `-0,0 +1 ` | test infrastructure |
| `test/fixtures/missing_fields.json` | `-0,0 +1,4 ` | test infrastructure |
| `test/fixtures/public_pool.json` | `-0,0 +1,8 ` | test infrastructure |
| `test/native_mining_validation.cpp` | `-0,0 +1,300 ` | test infrastructure |
| `test/native_poolstats.cpp` | `-0,0 +1,307 ` | test infrastructure |
| `test/native_stubs/Arduino.h` | `-0,0 +1,18 ` | test infrastructure |
| `test/native_stubs/esp_log.h` | `-0,0 +1,3 ` | test infrastructure |
| `test/native_stubs/esp_timer.h` | `-0,0 +1,3 ` | test infrastructure |
| `test/verify_helios_root.py` | `-0,0 +1,55 ` | test infrastructure |
| `tools/package_release.ps1` | `-0,0 +1,147 ` | file appendix |
| `tools/render_pool_panel.py` | `-0,0 +1,120 ` | build/release tooling |
| `tools/run_native_tests.ps1` | `-0,0 +1,70 ` | file appendix |
| `tools/validate_classic_sha_codegen.py` | `-0,0 +1,347 ` | build/release tooling |
| `vendor/mbedtls-tls/LICENSE` | `-0,0 +1,202 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/ORIGIN.md` | `-0,0 +1,17 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/common.h` | `-0,0 +1,365 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/constant_time_internal.h` | `-0,0 +1,335 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/ssl_cli.c` | `-0,0 +1,4397 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/ssl_msg.c` | `-0,0 +1,5739 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/ssl_srv.c` | `-0,0 +1,4631 ` | Pool providers, API and TLS |
| `vendor/mbedtls-tls/ssl_tls.c` | `-0,0 +1,7622 ` | Pool providers, API and TLS |
<!-- HUNK_INVENTORY_END -->
