# perf.9 local mining results

Unofficial NerdMiner_v2 modification. Comparison source: perf.8 commit
`1c5fc809c17daa0143cae737a343342dd968db5f`. Target:
ESP32_2432S028_2USB, classic dual-core Xtensa LX6, 240 MHz, 4 MB flash.
No physical board or serial port was accessed. Hardware performance is not
measured. This is a local release candidate, not a hardware-qualified release.

## Selected architecture and correctness boundary

Retain one unpinned software worker and one unpinned hardware worker, unchanged
priorities, 4096/16384 nonce ranges and 256-nonce cancellation checks. Preserve
the software engine, generation logic, queue ownership, completed-work counters,
independent reference validator, exact target comparator and Stratum serializer.

The HW range loop is extracted into an IRAM kernel. Its normal rejecting path
has no function calls. The three-compression sequential peripheral protocol is
unchanged. Input writes occur only when idle. No state-restoration or active
SHA_TEXT overlap is introduced. The old unsupported experiment stays compiled
out by default and is not present in the selected executable.

`src/crypto/ClassicEsp32ShaAccess.h` retains the official ESP-IDF 4.4.6 APB
pre-read before every DPORT read, interrupt protection and PS restoration,
without a function call per poll. Final polling and filter-word reading share
one protected interval. No call occurs while the saved PS must be restored.
The integer byte swap avoids a libgcc call and does not alter SAR. Its volatile
memory barrier keeps it after START, before BUSY polling: only CPU arithmetic
overlaps compression. Target disassembly confirms that ordering.

All input fills and control stores use one range-local register-window base.
Control offsets are derived from SDK register definitions, not magic numbers.
The range kernel alone uses -O2; all other firmware and software SHA keep -Os.
SW still reuses the first-block midstate and baked first three rounds. It
finishes the nonce-dependent first compression and uses its existing exact
partial-round second-SHA rejection; qualifying results finish the full digest.
None of that arithmetic or message scheduling changes in perf.9.

### Operations per ordinary HW nonce, before and after

| Operation | perf.8 | perf.9 |
| --- | ---: | ---: |
| SHA compression | 3 | 3 |
| LOAD | 2 | 2 |
| SHA_TEXT stores | 40 | 40 |
| Control-register stores | 5 | 5 |
| Idle polling loops | 5 | 5 |
| Digest words read on rejection | 1 | 1 |
| Protected-read helper calls | Present | None |
| Nonce byte-swap library call | Present | None |

BUSY reads depend on actual peripheral latency; five loops do not mean exactly
five status reads. Rare filter hits still read all eight digest words and use
the independent full candidate validation gate before submission. No SHA rounds
or message-schedule work were removed. SW SHA work is unchanged.

### Nonce sentinel correction

Previously `nonce != 0xFFFFFFFF` was used as the candidate-presence test. This
silently excluded a legitimate nonce. `JobResult::has_candidate` now distinguishes
absence from every possible 32-bit nonce. All worker candidate writers set it;
the coordinator tests it; experimental fallback clears it. No serialized nonce,
nTime, extranonce, job identifier or wallet behavior is changed.

JobResult grows from 136 to 144 bytes on this target, eight extra heap bytes per
result. Allocation/list architecture is otherwise unchanged. Existing best-per-
range candidate selection and bounded queues are not redesigned. This work does
not claim a new universal guarantee against all queue pressure or job expiry.

## Experiments performed in this revision

| Candidate | Correctness | Evidence | Decision |
| --- | --- | --- | --- |
| Inline protected BUSY polling | Target model and native regression pass | Initial worker 1079 -> 1023 bytes; avoids poll/restore calls | Retained |
| Small IRAM range kernel, fused final read, local MMIO base | Target model passes | No call on ordinary rejection path | Retained |
| Integer-only inline byte swap | Target model includes endian boundaries | No SAR mutation; no per-nonce libgcc call | Retained |
| Ordered CPU swap after START | Target model passes | Disassembly places eight arithmetic operations between START and polling; final kernel 656 bytes | Retained |
| Schedule-first SW: expand W before compression | 5,000,000 cases, 73 exact hits, zero mismatches | Host 1,816,584 vs 1,892,513 nonce/s; 16,056-byte hot function, 224-byte frame vs 112 | Rejected |
| Split SW first/second compression functions | 5,000,000 cases, 73 exact hits, zero mismatches | Host 1,897,102 vs 1,892,513 nonce/s (noise); 7592 + 8388 bytes; nested frames 144 + 368 | Rejected |
| Equivalent XOR majority expression | 5,000,000 cases, 73 exact hits, zero mismatches | Same 15,908-byte Xtensa hot function and 112-byte frame; host slower in paired checks | Rejected |
| Direct SAR-changing rotate/Sigma assembly | Not retained | GCC 8.4 rejects a SAR clobber; explicit SAR preservation adds work to already efficient rotations | Not implemented in production |
| Two current SW workers | Static architecture comparison only | Inferred 70--90 kH/s total versus roughly 340 kH/s hybrid | Not selected |
| Skipping padding MMIO stores | Documentation investigation only | No relied-upon documented retention guarantee across all operations | Not implemented |
| Affinity, priorities, range size | No new physical evidence | Existing settings retained; overhead is secondary to SHA/MMIO latency | Deferred hardware matrix |

Host rates are relative desktop signals, not ESP32 throughput. Rejected SW
source is removed from production; `src/ShaTests/nerdSHA256plus.cpp` has no
content change relative to perf.8. Earlier ring/partial-unroll experiments were
not repeated. Unsupported active-input overlap was not reconsidered as safe.

## Final-form compiler comparison

Before the final ordered-swap refinement, compiling the same SDK-derived control
addressing with each function flag produced:

| Flag | Kernel bytes | Frame bytes | Simulated instructions, 16384 nonce range |
| --- | ---: | ---: | ---: |
| -Os | 614 | 80 | 2,769,470 |
| -O2 | 660 | 112 | 2,671,202 |
| -O3 | 660 | 112 | 2,671,202 |

All three pass emitted-instruction/MMIO tests. -O2 removes approximately six
executed CPU instructions per nonce relative to -Os in this synthetic range.
-O3 provides no improvement. The final ordered-swap -O2 kernel is 656 bytes,
96-byte frame, with the same range instruction count. Select -O2 only here;
do not extrapolate to the software SHA translation unit.

The model uses artificial BUSY delays to exercise polling, not real peripheral
timing. These instruction counts are NOT CPU-cycle measurements. Mandatory
hardware latency may hide some of the removed instruction overhead.

## Resource comparison (target build and disassembly)

| Item | perf.8 | perf.9 | Delta |
| --- | ---: | ---: | ---: |
| Static RAM | 55,700 | 55,700 | 0 |
| Reported application flash | 2,144,257 | 2,144,321 | +64 |
| .iram0.text | 109,887 | 110,603 | +716 |
| HW worker bytes | 1079 | 419 | -660 |
| HW worker frame | 256 | 224 | -32 |
| New HW kernel bytes/frame | none | 656 / 96 | new |
| SW baked function bytes/frame | 15,908 / 112 | 15,908 / 112 | 0 |
| Application binary bytes | 2,150,560 | 2,150,624 | +64 |
| Merged factory bytes | 2,220,032 | 2,220,032 | 0 (alignment) |

Worker plus nested kernel frames require 320 bytes versus the old worker's
256-byte frame, before other callees. HW task allocation remains 3584 bytes.
This is not a stack high-water measurement. Heap, largest free block, interrupt
latency, task responsiveness and watchdog behavior require hardware validation.
Existing framework/display warnings and unchanged SW unused-variable warnings
remain; no warning was observed in the new kernel/access code.

## Tests and evidence

- Fresh native provider selection/parser/policy/cache/backoff suite: PASS.
- Fresh native mining suite: 5,000,000 deterministic headers, exact filter
  agreement on every case; all 73 qualifying full digests byte-identical to
  independent reference SHA-256d. Nonqualifying full SW digests are deliberately
  not produced by the early-return engine; do not describe them as five million
  full-digest comparisons.
- SHA empty/abc and SHA-256d empty vectors, genesis and three historical block
  headers: PASS. All target byte positions, equality/endian boundaries: PASS.
- 10,000 forced exact-target candidates, corrupt candidates and stale
  generations: PASS. 100,000 range allocations and rapid generations: PASS.
- `tools/validate_classic_sha_codegen.py` executes actual emitted Xtensa
  instructions against a strict independent hashlib/MMIO model. It is NOT a
  full ESP32 emulator. SHA math is supplied by desktop hashlib; it verifies
  input construction, padding, endian handling, protected reads, idle-before-
  write, exact filter decisions, actual rare candidate stores and PS restoration.
- Final kernel model: 1,000,015 headers, 16 qualifying full-digest hits, zero
  mismatches. Includes a genuine filter hit at nonce 0xFFFFFFFF, not a forged
  digest. Also tests 4096/16384 contiguous ranges across nonce wrap, and
  cancellation yielding exactly 1/257/257 completed nonces without gaps.
- Clean release build for ESP32_2432S028_2USB: PASS. Physical mining, pool
  acceptance and hardware peripheral behavior remain untested in this revision.

## Honest performance model

The only physical input is the user's approximately 340 kH/s combined baseline.
SW 35--45 and HW 295--315 kH/s are inferred decomposition, not measured worker
rates. At nominal HW 300 kH/s, 240,000,000 / 300,000 = 800 CPU cycles per nonce.

If the removed calls/cache traffic/restore overhead saves a net 60--130 cycles
per nonce, the model gives HW 324--358 kH/s, plus unchanged SW 35--45 kH/s.
At a nominal 100-cycle saving: 240,000,000 / 700 + 40,000 = 382,857 nonce/s.
Round the single best estimate to **385 kH/s**, about **13% above 340 kH/s**.

The cycle saving is an engineering assumption, NOT a measured or fully modeled
cycle count. Call removal partly overlaps hardware latency and cache effects
are not calibrated. Conservative combined expectation: 340--365 kH/s;
realistic: 370--400; optimistic: about 420. A zero gain cannot be excluded until
hardware testing. No 600/700/800 kH/s claim is supported by retained code.

With SW around 40 kH/s, 700 total needs HW around 660 kH/s (364 cycles/nonce),
versus the inferred current 800. The unchanged three compressions, two LOADs
and forty input writes make that an unsupported doubling assumption. Two
current SW workers are worse. No replacement SW candidate passed the target
performance gate. This is the best locally supported candidate from these
experiments, not proof that every possible LX6 design has been exhausted.

## Later physical acceptance gate

Compare perf.8 and perf.9 at 240 MHz with identical pool, active UI and Wi-Fi.
Measure long-window unique completed work, SW/HW split, accepted/rejected/stale
shares, validation-error count, cancellation under rapid notifications, task
stack high-water marks, heap minima, watchdogs and TLS/SHA-lock coexistence.
Reject the candidate if throughput fails to improve or stability/share quality
regresses. Affinity/core mapping is a separate later isolated experiment.

## Documentation and license basis

The classic ESP32 TRM chapter 16 documents START/CONTINUE, BUSY=0 before LOAD
and BUSY=0 before digest reading. Accelerator-clock latency is not converted
to CPU-clock latency without establishing the clock domain.

- https://documentation.espressif.com/esp32_technical_reference_manual_en.pdf
- https://github.com/espressif/esp-idf/blob/v4.4.6/components/esp_hw_support/port/esp32/dport_access.c
- Installed ESP-IDF soc/dport_access.h, hal/sha_ll.h and xtensa/xtruntime.h.

Upstream MIT license is unchanged. Adapted protected-read code retains
Espressif copyright and Apache 2.0 attribution; full license is supplied in
LICENSES/Apache-2.0.txt and referenced by THIRD_PARTY_NOTICES.md.
