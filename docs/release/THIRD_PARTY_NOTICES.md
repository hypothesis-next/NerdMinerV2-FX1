# Third-party notices

The unmodified TLS runtime files in `vendor/mbedtls-tls` are from Espressif's
mbedTLS 2.28.4 submodule, commit `1d7033af30e20ccb2a0c0a114d3a08e372430f34`,
selected by ESP-IDF v4.4.6. See that directory's `ORIGIN.md` for provenance and
its original Apache 2.0 `LICENSE`. This source inclusion is not a framework
security upgrade; original copyright notices remain intact.

The protected classic-ESP32 DPORT access in
`src/crypto/ClassicEsp32ShaAccess.h` is derived from ESP-IDF v4.4.6
`components/esp_hw_support/port/esp32/dport_access.c`:
https://github.com/espressif/esp-idf/blob/v4.4.6/components/esp_hw_support/port/esp32/dport_access.c
Copyright 2010-2021 Espressif Systems (Shanghai) CO LTD; Apache License 2.0.
The modification inlines protected polling and combines the final idle check
with a protected digest-word read. Its notices are retained in the header and
the complete license is included in `LICENSES/Apache-2.0.txt`.

This unofficial modification retains the upstream NerdMiner_v2 source,
libraries, assets, license files, and their existing notices.

The embedded TLS trust bundle is generated from the Mozilla CA certificate
collection distributed by certifi. certifi is made available under the Mozilla
Public License 2.0. The certificates remain subject to the terms of their
respective certificate authorities.

- certifi: https://github.com/certifi/python-certifi
- Mozilla Public License 2.0: https://www.mozilla.org/MPL/2.0/

The bundle format was generated with the ESP-IDF certificate-bundle utility.
ESP-IDF is provided under the Apache License 2.0 with some components under
compatible licenses; see https://github.com/espressif/esp-idf.

Other libraries compiled for the target are retained under their original
licenses and copyright notices:

- TFT_eSPI, including Adafruit-derived display/font material: bundled
  `LICENSES/TFT_eSPI-license.txt` and `LICENSES/Adafruit-GFX-license.txt`.
- WiFiManager (tzapu), ArduinoJson (Benoît Blanchon): MIT licenses in
  `LICENSES/WiFiManager-LICENSE.txt` and `LICENSES/ArduinoJson-LICENSE.txt`.
- OneButton (Matthias Hertel): BSD-style license in
  `LICENSES/OneButton-LICENSE.txt`.
- NTPClient (Fabrice Weinberg): MIT notice in
  `LICENSES/NTPClient-MIT.txt`.
- TFT_eTouch (Achill Hasler): Boost Software License 1.0 in
  `LICENSES/Boost-1.0.txt`; original source headers retain copyright notices.
- OpenFontRender / FreeType: `LICENSES/OpenFontRender-LICENSE.txt` and
  `LICENSES/FreeType-FTL.txt`. Portions of this software are copyright
  © The FreeType Project (www.freetype.org). All rights reserved.

The source archive contains the upstream project MIT license and included
library/source notices. Libraries fetched by PlatformIO retain their own
upstream source and license terms. No affiliation with these licensors is
implied.
