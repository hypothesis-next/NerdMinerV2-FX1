# Performance-test methodology

V1.8.3-multipool-perf.8 is an unofficial local release candidate. It does
not claim that one megahash per second has been achieved.

## What is validated on the host

- SHA-256 standard vectors and SHA-256d vectors.
- The byte-exact Bitcoin genesis block header and digest, plus historical block
  headers at heights 1, 100000, and 700000.
- Little-endian 256-bit target comparison at every first-differing byte.
- Fixed-seed differential comparison between the optimized mining SHA and an
  independent reference implementation.
- Exact early-filter acceptance and rejection behavior.
- Stale generation and deliberately corrupted candidate rejection.
- Ten thousand forced exact-target candidates with matching, corrupted, and
  stale variants, including the 32-bit generation wrap boundary.
- Sequential mixed-size nonce ranges without overlaps or gaps.

The normal deterministic run uses five million randomized 80-byte headers.
Host throughput is useful only for comparing host compiler variants; it is not
an estimate of ESP32 throughput.

`tools/run_native_tests.ps1` compiles both native suites from the current
source, so the mining result cannot come from a previously built executable.

## What must be measured on ESP32_2432S028_2USB

1. Record chip revision, CPU frequency, firmware version and power supply.
2. Run at least 30 minutes before recording local hashrate.
3. Compare at least 24 hours of completed nonce accounting with elapsed time.
4. Confirm submitted shares pass the independent validator and are accepted.
5. Compare pool-side estimated work over a statistically meaningful interval.
6. Exercise display changes, Wi-Fi reconnect, rapid new jobs and statistics API
   failures while mining.
7. Record free/minimum heap, task stack high-water marks and watchdog resets.

Only unique, reference-correct nonce attempts count as performance. Displayed
hashrate alone is not proof.

## Compiler experiment

The target's original `-Os` hot routine is 15,955 bytes with a 112-byte stack
frame. Isolated `-O2` and `-O3` generated the same 20,110-byte routine with a
160-byte frame. With no physical timing evidence to justify the increased IRAM
and stack pressure, the release retains `-Os`.

The new classic-ESP32 hardware pipeline was also compiled in isolation. Its
`-Os` hot routine is 642 bytes with a 48-byte frame. Per-function `-O2` grew it
to 781 bytes/64 bytes, and `-O3` to 897 bytes/96 bytes, with more spills. The
candidate therefore retains `-Os` for this routine as well.

## Physical-baseline attribution

The only measured figure available for the target board is the user's combined
340--350 kH/s observation. The current firmware has no independent SW/HW
throughput counters, so an exact physical split cannot be recovered from that
number.

The earlier 140--150 kH/s SW estimate was produced by subtracting the historical
`~200KH/s` hardware microbenchmark comment in
`src/ShaTests/nerdSHA_HWTest.cpp` from the combined board result. That is not a
valid decomposition: the historical microbenchmark reads the complete digest
for every nonce, while the production hardware worker normally reads only the
single word needed by its exact early filter.

The same source records approximately 39--41 kH/s for the specialized software
SHA routine. Its target `-Os` disassembly is a 15,955-byte, 112-byte-frame,
fully unrolled routine. At 240 MHz, 39--41 kH/s corresponds to approximately
5,850--6,150 CPU cycles per nonce. In contrast, 140--150 kH/s would allow only
1,600--1,714 cycles for roughly 121 SHA rounds plus schedule work and is not
supported by this implementation's generated code.

The best current source-based estimate is therefore 35--45 kH/s for the
software worker. If both workers were continuously active during the measured
340--350 kH/s run, residual attribution suggests approximately 295--315 kH/s
for the production hardware worker. That latter number is an inference, not an
independent measurement. The different 4,096/16,384 batch sizes allocate work;
they do not establish a 1:4 throughput ratio.

## Classic ESP32 hardware-pipeline experiment

The legacy worker exposes all 40 SHA text-register writes around three hardware
compressions for every nonce. The experimental path overlaps 16 second-block
writes with the first compression, eight double-SHA padding writes with the
second compression, and eight next-header writes with the third compression.
Only the remaining eight next-header writes are necessarily exposed after the
digest read.

ESP-IDF 4.4.6 explicitly says `SHA_TEXT_BASE` is shared and all SHA engines must
be idle before that memory is modified. START, CONTINUE and LOAD are simple
control-register writes; neither the HAL nor the ESP32 technical reference
manual documents an input-latched state in which SHA_TEXT may be overwritten
while BUSY is asserted. The perf.5 overlap therefore cannot exclude a silent
false rejection and is disabled by default in perf.6.

The old path remains available only behind the explicit development macro
`NERDMINER_EXPERIMENTAL_SHA_TEXT_OVERLAP=1`. Sampling or probation can detect
observed mismatches but cannot prove that a rare valid share was not turned into
a rejection. It is not suitable as a public-release default.

The selected path modifies SHA_TEXT only after BUSY clears. It safely overlaps
the CPU-only nonce byte swap with the first compression and also waits for the
final LOAD operation to become idle before reading the digest. Larger staging
buffers do not hide the dominant MMIO writes because those writes still cannot
begin until idle. An exact independent computation of the final filter word
requires essentially the full SHA-256 state and is not a cheap per-nonce guard.

## Architecture experiment policy

Worker affinity, two-software-worker mode and hardware-midstate restoration are
not selected based on static estimates. Two current software workers cannot
plausibly replace the inferred approximately 295--315 kH/s hardware contribution, and classic
ESP32 has no supported SHA-state restore operation. Affinity and priority remain
unchanged until measured on hardware.

## Perf.8 documented-contract experiments

The retained classic-ESP32 worker still performs exactly three SHA compression
operations, two LOAD operations, 40 SHA_TEXT writes, five control writes and
five BUSY waits per nonce.  None of the retained changes writes SHA_TEXT while
BUSY is asserted.

The BUSY loop uses ESP-IDF's documented sequence-read form while interrupts are
disabled by the caller.  The SHA_TEXT fill helpers also keep the register-window
base in one Xtensa address register.  Target disassembly reduced
`minerWorkerHw` from 1,123 to 1,079 bytes and removed approximately 37 repeated
absolute-address materializations.  Static RAM and the 256-byte worker frame
are unchanged.  This is a disassembly result; its physical throughput effect
still requires an A/B test on the target board.

The software job bake now stores the constant contributions to W18 and W19.
Per nonce, W18 requires only its nonce-dependent sigma term and W19 only its
nonce addition.  Five million deterministic differential cases passed.  The
`nerd_sha256d_baked` frame remains 112 bytes and its symbol decreased from
15,955 to 15,908 bytes.  This removes useful work but represents only a small
fraction of the approximately 5,850--6,150 cycles per software nonce.

Two structurally different software engines were evaluated and rejected:

| Candidate | Correctness | Host signal | Xtensa code / frame | Decision |
| --- | --- | --- | --- | --- |
| 16-word circular schedule | 5,000,000 cases | about 17% slower | 891 B / 192 B, plus 256 B DRAM constants | Rejected |
| 8-round partial unroll with linear W[64] | 5,000,000 cases after alignment correction | about 6.3% slower | 7,103 B / 368 B | Rejected |

The first partial-unroll draft began an eight-round group at round 18 and was
correctly rejected by the differential test.  Aligning grouped rounds at 24
made it byte-exact, but not faster.  Both experiments were removed from the
production source.

The current Xtensa routine has no per-nonce helper calls and only two branches,
but its 5,826 decoded instructions contain 374 stack loads and 112 stack stores.
Constant rotates already compile to the LX6 `ssai`/`src` pair; replacing those
expressions with small inline assembly cannot reduce the two-instruction rotate
primitive.  W16/W17, rounds 0--2 and the constant parts of W18/W19 are now
precomputed.  From W20 onward the schedule depends nonlinearly on the nonce, and
the second SHA depends on the entire first digest, so no further comparable
per-job schedule split was identified.

At the inferred 305 kH/s hardware contribution, the complete hardware path
costs about 787 CPU cycles per nonce.  A 660 kH/s hardware worker (needed for
700 kH/s combined with the current software worker) would have only 364 cycles
for the three compressions, two LOADs, 45 writes, five waits and loop work.
The safe address-generation change may plausibly recover a few percent, not a
factor of two.  Until measured, the engineering expectation for perf.8 is
roughly 340--380 kH/s combined, with 350--365 kH/s the realistic band.  These
are static estimates, not measurements.
