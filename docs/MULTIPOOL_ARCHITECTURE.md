# Multi-pool statistics architecture

## Scope and invariants

Stratum mining remains generic and is not coupled to dashboard APIs. Pool
statistics are optional display data. A provider failure does not request a
Stratum reconnect, acquire a mining lock, or change mining state.

The implementation has three layers:

1. `PoolRegistry` normalizes the configured Stratum hostname and selects an
   explicit provider definition.
2. `PoolStatsProvider` performs provider-specific HTTP/TLS and parsing into a
   generic `PoolStatsSnapshot`.
3. `PoolStatsService` schedules requests in a separate low-priority task and
   publishes snapshots to display drivers through a short dedicated critical
   section.

Display drivers consume only the pool name, generic values, and metric state.
They contain no endpoint or JSON-field knowledge.

## Provider registry

| Normalized Stratum host | Display name | Provider |
| --- | --- | --- |
| `public-pool.io` | Public Pool | Public Pool-compatible |
| `btc.heliospool.com` | HeliosPool | Helios snapshot |
| `pool.nerdminers.org` | NerdMiner Pool | Public Pool-compatible |
| `pool.sethforprivacy.com` | Seth for Privacy | Public Pool-compatible |
| `pool.solomining.de` | SoloMining.de | Public Pool-compatible |
| `tn.vkbit.com` | TESTNET | Existing static TESTNET behavior |
| any other host | normalized host, truncated when required | none |

Provider selection is hostname-based. An unrecognized host receives no API URL
even if it uses a port commonly associated with another pool implementation.

## HeliosPool mapping

The official HeliosPool `ckstats-lhr` source was rechecked at commit
`33717e93a392c4d5c7b602f747843ffceb53231f` (2026-07-27). The route
`app/api/users/snapshot/route.ts` implements:

`GET /api/users/snapshot?address=<BTC_ADDRESS>`

It returns the user with the latest stats record and its workers, emits
`Last-Modified`, accepts `If-Modified-Since`, and can return 304, 400, 404, or
500. Source:
https://github.com/heliospool/ckstats-lhr/blob/main/app/api/users/snapshot/route.ts

The provider uses:

| Dashboard label | Snapshot field/derivation | Meaning |
| --- | --- | --- |
| Best Ever | `stats[0].bestEver` | User's historical best-ever share difficulty |
| Workers | count of `workers[].lastUpdate` within 24 hours of HTTP `Date` | Active-worker definition used by the Helios UI |
| Total Hash Rate | `stats[0].hashrate5m` | User aggregate five-minute hashrate |

`bestEver` is deliberately selected instead of `bestShare`. The UI label is
therefore `Best Ever`, avoiding an implication that the value is only the
current sampling period's best share.

The 24-hour rule is defined by the official
`utils/workerActivity.ts` implementation. The firmware uses the response's
authenticated HTTP `Date` rather than the ESP32 wall clock, avoiding a new time
synchronization dependency for worker counting.

## Failure behavior

- Successful refresh interval: 60 seconds.
- Data is considered stale after 180 seconds without success.
- Failure backoff: 15, 30, 60, 120, 240, then 300 seconds.
- HTTP 429 backoff: 300 seconds immediately.
- Wi-Fi-disconnected recheck: 5 seconds; no HTTP attempt is made.
- Connect timeout: 4 seconds.
- Read timeout: 5 seconds.
- TLS handshake timeout: 5 seconds.
- Maximum response: 16 KiB.
- Minimum heap gate before a request: 45,000 bytes free and a 24,000-byte
  largest allocatable block.

HTTP 200 must parse all required fields. HTTP 304 refreshes the age of an
existing cache. HTTP 400/404 produces an explicit unavailable state. HTTP 429,
500/503, DNS/connect/TLS failures, timeouts, malformed JSON, missing fields,
and oversized responses use failure backoff. A last-good snapshot is retained
and shown as `STALE`; without a cache, values are `N/A` with `ERROR`.

TLS endpoints use the embedded ESP x509 certificate bundle derived from the
Mozilla trust store. Insecure TLS mode is not used.

## Local versus remote difficulty

The main mining screen's local best difficulty is unchanged and remains part of
the mining statistics path. The lower panel's `Best Ever` is a separate remote
field in `PoolStatsSnapshot`. Changing pool providers does not reset, overwrite,
or reinterpret local saved mining statistics.
