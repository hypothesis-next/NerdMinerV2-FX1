# Changelog

## V1.8.3-FX1 RC1 — unofficial release candidate

This release is based on NerdMiner V2 V1.8.3. The public firmware binary is
validated for **ESP32_2432S028_2USB**; other board variants are not covered by
the physical results below. The full stock-to-FX1 source inventory, including
inherited import differences and small cosmetic edits, is in
[DETAILED_CHANGES.md](docs/DETAILED_CHANGES.md).

### Added

- Pool-specific statistics providers with HeliosPool support, separate from
  Stratum mining. Unknown pools continue to mine without making a substituted
  Public Pool request.
- Independent reference SHA-256d validation of share/block candidates and
  native tests for hashing, target boundaries, job transitions, nonce ranges,
  PoolStats parsing/policy and certificate provenance.
- A configuration-preserving browser updater that writes the tested application
  image only at `0x10000` on compatible layouts; the factory installer is
  deliberately separate and marked as configuration-destructive.

### Mining correctness and safety

- Correct full-width 256-bit share/network target comparison and target-byte
  order; verified the exact reconstructed 80-byte header before submission.
- Full-width job generations, stale-result protection, immutable candidate
  snapshots, exact range continuation after candidates and correct handling of
  nonce `0xFFFFFFFF`.
- Synchronized long-running completed-hash counters and partial-range
  accounting; submission now checks whether the complete Stratum message was
  written. Wallet, extranonce, job ID and nTime remain tied to the job.
- Classic-ESP32 SHA command ordering and DPORT synchronization corrected after
  real-device differential testing exposed digest mismatches. Unsupported
  active SHA input-register overlap is not used by the release path.

### Pool statistics, TLS and networking

- Replaced the display's direct Public Pool fetch with a bounded, cached,
  provider-based statistics task and explicit stale/error/backoff behavior.
- Updated Helios to the deployed `/api/users/<address>` route and current
  response structure. Best Ever is an account-level submitted-share metric;
  Total Hash Rate is the pool's recent estimate, not the device counter.
- Retained CA and hostname verification. Helios uses its dedicated current
  ISRG Root X2 anchor; compatible other providers retain the generic bundle.
  NTP validity is checked before HTTPS. There is no insecure fallback.
- Coordinated TLS and hardware mining through documented SHA/DPORT access and
  bounded CPU windows, so a statistics request no longer forces the hardware
  worker into software-only hashing for the whole request.
- Bounded the large Helios response drain while retaining a verified reusable
  HTTPS connection. Reduced TLS memory pressure; failures remain isolated from
  Stratum mining.

### Display, memory and build

- Corrected a Merkle-root terminator out-of-bounds write, PoolStats TLS task
  stack sizing and several display-buffer/state issues found during testing.
- Kept the existing UI style while fitting Helios values in the CYD panel and
  positioning the startup label `V1.8.3-FX1` clear of QR/logo/status elements.
- Added build, test, licensing, checksum and source-archive materials for this
  unofficial fork. The release binary is not rebuilt by the web installer.

### Physically measured result

On one ESP32_2432S028_2USB, a 448.72-second post-warm-up run completed
195,645,712 hashes (**436.01 kH/s**) with 227 accepted / 0 rejected shares,
zero candidate-validation errors and six successful Helios refreshes. The exact
public application was subsequently browser-flashed and read back byte-for-byte;
its short 178.633-second measurement completed 78,106,120 hashes
(**437.24 kH/s**), with 111 accepted / 0 rejected shares and three successful
Helios refreshes. The stock reference on the same board was approximately
340–345 kH/s. These are single-device observations, not a promise for other
boards or networks. See [hardware validation](docs/release/HARDWARE_VALIDATION.md).

### Known limitations

- TLS-time free heap has fallen to a few kilobytes in testing; prolonged uptime
  and unusual network/certificate responses need continued observation.
- Only the named classic-ESP32 board, one account/network and short display
  off/on intervals were physically checked. The former overnight statistics
  failure was reproduced as an obsolete API route, but multi-day operation
  after the repair has not been demonstrated.
- The source retains some development-only guarded experiments for auditability;
  unsafe SHA register overlap is disabled in the public build. Invalid high
  experimental hashrates are not release performance.

## Earlier local development history

The pre-release `multipool` and `perf` candidates were internal checkpoints,
not separate public FX1 releases. Their implementation, rejected experiments,
physical failure investigations and validation evidence are summarized in
[DETAILED_CHANGES.md](docs/DETAILED_CHANGES.md) and the linked technical reports.
