# Helios refresh diagnosis and validation

Unofficial NerdMiner_v2 modification; no upstream or pool endorsement.

## API and metric semantics

Read-only requests to the deployed service established:

- `/api/users/snapshot?address=<address>` returns HTTP 400 `Invalid address`.
- `/api/users/<address>` returns HTTP 200 with `user`, `poolStats`,
  `historicalStats` and `generatedAt`; `user` is the first root member.
- An invalid path with a valid query still fails; a valid path with an invalid
  query succeeds. Address lookup uses the path, not the old query parameter.
  Exact server implementation/deployment timing could not be established from
  the currently unavailable public backend repository. No wallet change is needed.

The provider reads `user.stats[0].bestEver` (dimensionless account-level lifetime
share difficulty), `hashrate5m` (pool-reported recent H/s), and counts worker
timestamps within 24 hours of the HTTP Date, matching the deployed user page.
The normal worker suffix is removed only for the statistics lookup.

Local Best Difficulty updates on an accepted-share response and is session-local
unless saved statistics are enabled. Local 11.763 and account Best Ever about
2,769 are not contradictory: the latter includes earlier accepted work for the
account. Neither value is rescaled to make them match.

The live API's historical hashrate series itself contained 936,000 H/s. Firmware
does not generate that spike. Pool five-minute/share-derived estimation differs
from the local rolling ten-sample display and exact completed-work measurement.
The backend's exact smoothing formula and the historical instant's local counter
state were not available, so that event's precise cause is not claimed proven.

## Secure transport and mining

The previous measured refresh took 5.97 seconds and reduced actual work from
447.18 to 37.26 kH/s by assigning the hardware worker software work throughout
HTTPS. The replacement keeps that worker on hardware SHA, uses the official SDK
software SHA ALT mode for statistics-task TLS contexts, and gives bounded CPU
windows only after SHA is idle and all shared-memory/DPORT protections are released.
No SHA_TEXT write occurs during active compression. MEMW remains mandatory.

The verified HTTPS socket is reused after fully draining a bounded response:
only up to 16 KiB of the compact first user object is retained, body transfer is
limited to 512 KiB and 30 seconds, and ordinary socket reads retain their timeout.
A closed verified session may be cached;
no second peer certificate tree is kept while the connection stays open.

TLS uses ISRG Root X2, mandatory certificate/hostname checks, P-256 ECDHE and
AES-128-GCM/SHA-256 for Helios. No insecure TLS or HTTP fallback exists.
Other compatible providers retain the generic CA bundle; unknown hosts make no
statistics request. UTC gating, completion-based 60-second refresh delay,
last-good/stale policy, bounded backoff and HTTP 429 behavior remain in force.

Matching, unmodified official mbedTLS TLS runtime sources use the supported
1024-byte output record limit; the input limit remains 16 KiB. Build-time SDK
header fingerprints reject ABI drift. See `vendor/mbedtls-tls/ORIGIN.md` and LICENSE.

Cold handshakes temporarily borrow reusable rendering scratch, retaining the
last complete frame, then restore rendering before the HTTP body. Warm refreshes
do not borrow it. A new connection still costs CPU/certificate work; this is not
a promise that initial/reconnection handshakes always exceed 400 kH/s.

## Evidence and limits

The safe cooperative kernel passed 2,979,747 physical comparisons across normal,
record-I/O and handshake windows: 1,800,099 full digests plus 1,179,648 unforced
filter decisions, zero mismatch. The host regression suite passed five million
deterministic cases plus known headers, target/candidate/generation/range tests.
Rejected non-stalled DPORT alternatives produced physical mismatches and are not
enabled in this release.

Bulk body reads shortened transfer but reduced actual refresh throughput in a
controlled test. Removing record-I/O CPU windows caused a TG1 watchdog reset.
Both variants were rejected; the final transfer path retains bounded CPU windows.

The preceding keep-alive development build measured 447.07 kH/s over 899.364
seconds after warm-up, eleven successful refreshes, 321 accepted and zero rejected
shares; all regular refresh five-second minima exceeded 400 kH/s. The final build's
own measurements/checksum are recorded separately with the release artifacts.

Configuration is stored separately from the application and must never be erased
for this update. Local best, statistics cache, timestamps and backoff are RAM-only
when saved statistics are disabled. A reboot cannot disprove an overnight failure.
The old endpoint's reproduced HTTP 400 explains unavailable values and increased
backoff, but does not prove when the server behavior changed overnight.

Repeated refresh success and recovering heap do not prove absence of a very slow
leak. Multi-hour/multi-day runs, server disconnect/CA changes, intermittent Wi-Fi,
and rare pool-side statistical events remain longer-term validation obligations.
