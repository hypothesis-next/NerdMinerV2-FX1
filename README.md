# NerdMiner V2 FX1

[AI DISCLOSURE: USE OF CLAUDE OPUS 5.5 FOR THE TESTING, ITERATIONS AND CODING UNDER MY DIRECTION AND GUIDANCE]

An unofficial open-source fork of
[NerdMiner V2](https://github.com/BitMaker-hub/NerdMiner_v2) that advances the
firmware beyond the upstream V1.8.3 baseline. FX1 pursues higher real mining
throughput without sacrificing correctness or stability, alongside broader
pool support, better dashboards and usability, bug fixes, and continued
firmware development. FX1 is not an official BitMaker-hub, NerdMiner,
HeliosPool, or pool-operator release.

## About this fork (hypothesis-next)

Speed work on top of FX1, offered back upstream as pull requests. `main` here is FX1 unchanged
apart from this section; each change lives on its own branch, kept level with FX1's `main`.
None of the pull requests has been merged upstream yet.

**What we changed and why.** The SHA-256 hardware loop spent most of its time waiting on and
talking to the SHA engine, not hashing. Each change below does less of that, and each checks
itself at boot against real Bitcoin block hashes and falls back to FX1's own loop if it fails.

| Branch | Pull request | What it does | Gain |
|---|---|---|---|
| `classic-esp32-faster-sha-loop` | [#2](https://github.com/samkruzlic/NerdMinerV2-FX1/pull/2) | Classic ESP32: fewer SHA register writes per nonce, bookkeeping out of the locked loop, codegen-checker fix | ~+9.5 % |
| `cyd-screen-sleep` | [#3](https://github.com/samkruzlic/NerdMinerV2-FX1/pull/3) | CYD: optional screen sleep after 30 s, tap to wake; nothing is drawn while dark | ~+13 % while dark |
| `classic-esp32-timed-kernel` | [#4](https://github.com/samkruzlic/NerdMinerV2-FX1/pull/4) | Classic ESP32 rev 3 at 240 MHz: fixed measured waits instead of polling, next block written while the engine is busy, software spot-checks with a one-way fallback (builds on #2) | CYD ~468 → ~680 kH/s screen on, ~760 with screen sleep |
| `s3-c3-batched-sha` | [#5](https://github.com/samkruzlic/NerdMinerV2-FX1/pull/5) | ESP32-S3 / C3: 32 nonces per batch, fewer register writes; a fix for builds stalling on an unread USB console; optional T-Display S3 screen sleep | S3 ~250 → ~432, C3 ~262 → ~438 kH/s |

**Which build do I use?** Install [PlatformIO](https://platformio.org/), then:

```bash
git clone -b <branch> https://github.com/hypothesis-next/NerdMinerV2-FX1.git
cd NerdMinerV2-FX1
pio run -e <env> -t upload
```

| Board | Branch | Env |
|---|---|---|
| CYD, two-USB ESP32-2432S028 (classic ESP32 rev 3) | `classic-esp32-timed-kernel` | `ESP32_2432S028_2USB` |
| CYD with screen sleep (without the timed kernel) | `cyd-screen-sleep` | `ESP32_2432S028_2USB_SCREEN_SLEEP` |
| LilyGO T-Display S3 | `s3-c3-batched-sha` | `NerdminerV2`, or `NerdminerV2_SCREEN_SLEEP` |
| ESP32-C3 super-mini | `s3-c3-batched-sha` | `ESP32-C3-super-mini` |

`pio ... -t upload` keeps the board's settings (Wi-Fi, pool, wallet); a full flash erase sends it
back to its setup portal. Each build logs at boot which SHA loop it chose (`[SHA] ...`); a failed
self-test means it fell back to FX1's own, slower loop.

**If you only want the fastest CYD.** On the same CYD and the same test pool,
[HenrysCat/NerdMiner-Modded](https://github.com/HenrysCat/NerdMiner-Modded) (MIT) mined
~925 kH/s against ~680 for the timed kernel here: its hand-written loop leaves the other CPU core
running and spaces its register writes. On a T-Display S3 it is the other way round
(~317 against ~432-457 here). We measured one board of each, one hour each.

**How it was tested.** Host tests and FX1's emitted-code checker, each new check first run
against a deliberately broken version; on-chip differential soaks (every nonce compared with
FX1's own loop and spot-checked in software, up to 100 M nonces under five CPU/SPI/Wi-Fi loads);
a local test pool that re-checks every share; hour and 24 h runs on public pools.
Boards: ESP32-2432S028 "CYD" ×2 (classic ESP32 rev 3.1), LilyGO T-Display S3 and Waveshare
S3-Zero (ESP32-S3 rev 0.2), an ESP32-S3 devkit, an ESP32-C3 super-mini (rev 0.4), and an
ideaspark 1.14" board (classic ESP32 rev 1.0).

**Still open.**
- The timed kernel is enabled on rev 3 only and proven on rev 3.1. On a bench build the rev 1.0
  ideaspark behaves identically (same timing edges, 0 wrong in 25 M nonces), so the rev-3 gate
  could be relaxed once the full firmware has run on rev 1; rev 3.0 is untested.
- Its waits are measured, not documented, so it keeps its software spot-checks for good.
- Without the other-CPU stall, our kernels were only correct with every SHA register write
  spaced 2+ cycles apart, and then slower than the timed kernel; HenrysCat's loop shows a
  carefully scheduled no-stall loop can be much faster.
- Only one board per chip revision has been tested.

**Thanks** to [samkruzlic](https://github.com/samkruzlic) for FX1: its tests, its codegen
checker and its careful classic-ESP32 loop made this work possible to measure and prove; to
BitMaker for the original NerdMiner; and to HenrysCat for publishing NerdMiner-Modded's loop and
its notes on register-write spacing.

Done with Claude (Anthropic) under the fork owner's direction; please review it like any outside
contribution.

## V1.8.3-FX1 RC1

This is a release candidate (RC/Beta), physically validated on one
**ESP32_2432S028_2USB** with a classic ESP32 and 4 MB flash. Other board
variants have not received the same physical validation.

## Performance

On the tested board, stock NerdMiner sustained approximately **340–345 kH/s**.
The exact FX1 application binary completed **437.24 kH/s** in the final
browser-update smoke test; an earlier longer FX1 run measured **436.01 kH/s**.
These are on-device completed-work measurements, not pool-side estimates.
Your result may differ with hardware, jobs, network conditions, and display
activity.

## What Changed

- Independent SHA-256d validation of share and block candidates, corrected full-target/endian handling, and safeguards for job ownership and stale work.
- Unique nonce-range and completed-hash accounting, plus hardware-SHA correctness and stability fixes.
- Secure HeliosPool statistics for Best Ever, Workers, and Total Hash Rate, with provider-specific handling and no fallback to another pool's data.
- TLS and mining coexistence that avoids the former large periodic hashrate collapse during statistics refresh.
- Memory, dashboard layout, startup-label, and configuration-preservation improvements.

These are only the most visible highlights. FX1 also includes many smaller
correctness, networking, UI, testing, and release-tooling changes. For the
complete stock V1.8.3 versus FX1 engineering record, including every
documented changed file and diff block, see
[DETAILED_CHANGES.md](docs/DETAILED_CHANGES.md).

The mining address, reward destination, and Stratum job ownership remain
governed by the user's saved configuration.

## Supported / Tested Hardware

**Physically tested:** ESP32_2432S028_2USB, classic ESP32, 4 MB flash. This
RC should not be treated as validated for every board supported by upstream.

## Install / Update

The [Web Flasher](https://samkruzlic.github.io/NerdMinerV2-FX1/) provides a recommended
**Update / Keep My Configuration** path for compatible existing installations.
It flashes only the
[physically tested application image](docs/release/NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-application.bin)
at offset **0x10000**. Do **not** select “Erase device” if you want to keep
Wi-Fi, mining address, and pool settings. On a compatible partition layout,
those saved settings remain intact. Browser flashing requires an HTTPS-hosted
page and a Web Serial-capable desktop browser.

The separate factory image is for a clean installation or recovery and
**may erase existing configuration**. Read the
[manual flashing and rollback guide](docs/release/FLASHING.md) before using
it. The [release checksums](docs/release/SHA256SUMS.txt) identify the exact
packaged files.

## Documentation

- [Changelog](CHANGELOG.md) — concise release history
- [Complete engineering changes](docs/DETAILED_CHANGES.md) — stock V1.8.3 versus FX1, including tests and limitations
- [Hardware validation](docs/release/HARDWARE_VALIDATION.md) — physical test evidence
- [Manual flashing and rollback](docs/release/FLASHING.md)

## Known Limitations

This is an RC/Beta tested on one board. TLS refresh can leave a low free-heap
margin; very long-term statistics availability and other hardware variants
still need broader testing. Helios dashboard values depend on Helios's API,
and pool-side estimated hashrate can differ substantially from locally
completed work.

## Support FX1 Development

FX1 has involved extensive firmware development, debugging, performance work,
correctness validation, and physical hardware testing. If FX1 is useful to
you, optional donations can help fund development tools, test hardware, and
additional NerdMiner boards for validating future releases across more units
and revisions. Donations are never required.

- Bitcoin (BTC): `bc1qe4fjy02f5h9yhfzxmntk6246pm3whu6vuyvxsz`
- Ethereum (ETH): `0x7A9F37dda7F417625387dEF44268c1CA0B21D792`

Verify the address and network before sending. Cryptocurrency transactions
generally cannot be reversed.

## Upstream & Credits

FX1 is an unofficial fork based on
[NerdMiner V2 by BitMaker-hub and upstream contributors](https://github.com/BitMaker-hub/NerdMiner_v2).
Their original project and contributors retain their own credit; this fork
does not imply their endorsement. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md) for license and component
attribution.

## License

The fork preserves the upstream MIT license. Bundled components may have
their own terms, documented in [third-party notices](THIRD_PARTY_NOTICES.md)
and [license files](LICENSES/).
