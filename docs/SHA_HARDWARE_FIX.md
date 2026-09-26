# Classic ESP32 SHA synchronization fix

The perf.9-audit.1 hardware kernel failed on ESP32-D0WD-V3 revision 3.1.
The independent candidate validator correctly blocked submission.

## Root cause and isolation

The inlined assembly BUSY readers omitted a CPU memory-ordering barrier.
The preceding volatile START/CONTINUE/LOAD store had a compiler-generated
MEMW **before** the store, but no MEMW between that store and the assembly
BUSY load. The assembly `memory` clobber only constrains the compiler;
it does not complete outstanding CPU stores. Polling could observe idle
before the pending command was visible, allowing premature input writes or
digest reads. This was exposed by the faster -O2/IRAM path.

Physical isolation on the same public genesis header showed:

- Production kernel: first nonce 0xda54f700 returned an all-zero digest.
- Reference SHA-256d: eea484e55ce16bdd5cfbb1c92505902e06ff57281b82e314fb276b3c0074f778.
- SDK polling with the remaining -O2 operations: 4096/4096 passed.
- Separate final read alone, builtin nonce swap alone, or HAL control writes
  alone: still failed.
- -Os diagnostic clone: 4096/4096 passed; this was not a proof that -O2 was
  intrinsically incorrect.
- MEMW before every polling interval, retaining -O2 and fused read:
  4096/4096 passed.
- Adding MEMW at only one of the five command transitions was insufficient.
- First-SHA LOAD capture also showed zero instead of the reference digest:
  corruption already existed before the second SHA.
- Perf.8-equivalent SDK operation sequence: 4096/4096 passed. This tests the
  peripheral sequence, not the complete historical perf.8 firmware.

## Fix and regression gate

`ClassicEsp32ShaAccess.h::waitIdle()` and `waitIdleAndReadFinalWord()` now
begin with MEMW. SHA mathematics, the filter, job ownership, credentials,
Stratum serialization and production counters are unchanged.
The target instruction model now requires a MEMW after each SHA command
before its BUSY read. The old audited ELF fails this new gate; the fixed ELF
passes. The model is not a hardware emulator and previously did not model
write ordering, explaining why the old host check missed the defect.

The explicit `ESP32_2432S028_2USB_SHA_DIAG` environment runs before network
and configuration initialization. It tests the actual production kernel,
not a replacement implementation. It compares every forced full digest,
then every real filter decision and validates candidates with the independent
reference implementation. It is absent from the normal target binary.

Physical fixed-kernel result: 993249 checked nonce executions, zero mismatches:
600033 full-digest checks and 393216 unforced filter checks across genesis and
two historical headers, including boundary nonces. Nineteen filter hits were
observed across both phases. Fresh host differential result: 5000000 headers,
73 exact-filter hits, zero mismatches.

The original full-flash backup remains preserved. Only the application at
0x10000 was written; no factory image or full-chip erase was used.
Long-term stability and additional hardware models remain separate tests.
