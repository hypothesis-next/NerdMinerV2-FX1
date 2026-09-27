# Official TLS runtime sources

Unmodified `ssl_cli.c`, `ssl_srv.c`, `ssl_msg.c` and `ssl_tls.c` from Espressif's
mbedTLS 2.28.4 submodule at commit `1d7033af30e20ccb2a0c0a114d3a08e372430f34`,
as selected by ESP-IDF v4.4.6. Original copyright headers and LICENSE are retained.

Source: https://github.com/espressif/mbedtls/tree/1d7033af30e20ccb2a0c0a114d3a08e372430f34

Only the ESP32_2432S028_2USB environments compile these files. The supported
`MBEDTLS_SSL_OUT_CONTENT_LEN=1024` setting reduces idle HTTPS memory; incoming
records remain 16384 bytes. All TLS runtime sources use the same configuration.
SDK cryptographic primitives, certificate verification, entropy and SHA ports
remain unchanged. The SDK `ssl.h` is byte-identical to this source revision
(SHA-256 `1d2d56b9c22e7c10754ac20cdf4bccc12a2a5380dad36e4910b1d40e40f238d1`).

Do not use these sources with a different framework version without checking
the SDK ABI and submodule revision. This is not a framework security upgrade.
