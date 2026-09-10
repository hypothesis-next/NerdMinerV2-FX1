# TLS trust bundle

`x509_crt_bundle.bin` is an ESP-IDF certificate bundle generated from the
Mozilla CA certificate collection distributed by certifi 2026.7.22. It is
embedded in firmware so recognized HTTPS statistics providers validate server
certificates. The firmware never disables TLS certificate validation.

The bundle was generated with Espressif ESP-IDF v4.4.6
`components/mbedtls/esp_crt_bundle/gen_crt_bundle.py` to match the Arduino-ESP32
2.0.14 framework used by this project.

Sources:

- https://github.com/certifi/python-certifi
- https://github.com/espressif/esp-idf/tree/v4.4.6/components/mbedtls/esp_crt_bundle
