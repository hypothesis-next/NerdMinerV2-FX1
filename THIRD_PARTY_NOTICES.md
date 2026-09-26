# Third-party notices

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
