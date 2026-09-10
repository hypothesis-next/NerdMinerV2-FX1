# Pool statistics tests

`native_poolstats.cpp` exercises the platform-independent production registry,
JSON parsers, HTTP-status policy, cache state transitions, and retry policy.
The JSON files in `fixtures` contain synthetic data only.

The test is intentionally desktop-native: it provides deterministic coverage
without requiring network access or a connected board. Network, TLS, display,
and FreeRTOS behavior still require target-board validation.
