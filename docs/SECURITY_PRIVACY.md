# Security and privacy review

The multi-pool change introduces no telemetry, analytics, API keys, remote
configuration, or tracking identifiers.

New outbound statistics connections are selected only for recognized Stratum
hostnames in `PoolRegistry.cpp`. An unknown pool has no statistics endpoint.
The wallet portion before an optional `.worker` suffix is sent only to that
recognized pool's statistics endpoint. The configured Stratum service continues
to receive the existing Stratum username and password as in upstream firmware.

HTTPS uses the embedded Mozilla-derived certificate bundle. Certificate
verification is not disabled. Wallet strings are restricted to alphanumeric
characters before use in a statistics URL, URLs are fixed-size, response data
is limited to 16 KiB, and only required JSON fields are retained.

Synthetic test fixtures contain no real credentials, wallet addresses, Wi-Fi
details, or API keys. Release packaging excludes build directories and local
logs.

The original firmware's existing network services and behavior outside this
pool-statistics change are unchanged and should be reviewed separately under
the upstream project's security model.
