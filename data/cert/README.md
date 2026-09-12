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

## HeliosPool trust anchor

`gts_root_r4.pem` is the self-signed GTS Root R4 certificate used only by the
HeliosPool statistics provider. It was retrieved from the official Google Trust
Services root collection on 2026-09-12:

- Source: https://pki.goog/roots.pem
- Subject and issuer: `CN=GTS Root R4,O=Google Trust Services LLC,C=US`
- Serial: `203E5C068EF631A9C72905052`
- Validity: 2016-06-22 00:00:00 UTC through 2036-06-22 00:00:00 UTC
- DER SHA-256 fingerprint:
  `349DFA4058C5E263123B398AE795573C4E1313C83FE68F93556CD5E8031B3C7D`
- Committed PEM SHA-256:
  `7E8B80D078D3DD77D3ED2108DD2B33412C12D7D72CB0965741C70708691776A2`

The certificate is embedded as a null-terminated text asset. Hostname and
certificate validity verification remain enabled. Other providers continue to
use `x509_crt_bundle.bin`; the firmware never falls back to insecure TLS.
