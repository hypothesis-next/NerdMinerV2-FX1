# Clean-build versus physically tested application

- Source commit: `33bd54f9077f7b31dd40ee776637dfc1f1df17c9` on `hardware-helios-refresh`.
- The physical smoke-test binary was built from the same working-tree source content immediately before the two startup-screen edits were committed. The commit contains **only** `src/version.h` and `src/drivers/displays/esp23_2432s028r.cpp`; no source was edited between that smoke test and the commit.
- Physically tested application SHA-256: `8645a861506ee324b51f718a1377fe3d40e6ec0e78d9df1763039342f35b8cd5`.
- Fresh clean committed-build application SHA-256: `1cb16e3586a2a7cfd5cfc666c21b4102ca9709c81f71dd2fc09052cf0552a3e5`.
- Both images are 2,160,992 bytes. Byte comparison found exactly **70 changed bytes** in three 4 KiB blocks. The only changed non-checksum data is five bytes of the embedded compile-time string: `Sep 28 2026 14:27:11` versus `Sep 28 2026 18:12:44`.
- `WiFiManager.cpp` in the pinned build dependency constructs that string from `__DATE__ " " __TIME__` (line 2312). Its timestamp change alters the 32-byte `esp_app_desc_t::app_elf_sha256` field at image offset `0xB0` and the final one-byte image checksum plus 32-byte validation hash at the image tail. All other image bytes, including executable instructions and other data, compare identically.

The build is **not bit-for-bit reproducible across build times** because this dependency embeds compile time. The difference is understood; it is not a mining, TLS, PoolStats, Stratum, wallet or configuration source change. The clean-build binary passed offline image/layout checks. After packaging, that **exact** public application was also installed through the browser updater, read back byte-for-byte (matching SHA-256 above), and smoke-tested with preserved configuration, live mining and Helios HTTPS. See `HARDWARE_VALIDATION.md`. No new firmware was built for that physical test.
