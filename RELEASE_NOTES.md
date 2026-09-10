# V1.8.3-multipool.1 release notes

This is an unofficial NerdMiner_v2 modification based on V1.8.3. It is not an
official or endorsed release of NerdMiner, BitMaker-hub, HeliosPool, or any
other pool operator.

The release removes misleading fixed Public-Pool.io dashboard behavior. A
recognized pool selects an explicit statistics provider; an unknown pool has
no remote provider and displays `N/A` while Stratum mining continues normally.

HeliosPool support maps:

- **Best Ever** to the latest user snapshot's `bestEver` value.
- **Workers** to workers whose `lastUpdate` is no more than 24 hours old.
- **Total Hash Rate** to the latest user snapshot's `hashrate5m` value.

Statistics run independently of mining. Requests are limited to 16 KiB, use
certificate validation, have bounded connection/read/TLS timeouts, refresh at
approximately 60 seconds after success, and back off to at most five minutes
after repeated failures. Cached values are marked `STALE`; absent or
unsupported data is shown as `N/A` rather than a fabricated zero.

Automated native tests and the `ESP32_2432S028_2USB` firmware build pass. UI
images are render mocks based on the real asset, dimensions, colors, and layout;
they are not hardware-emulator captures. Physical-board validation remains
required, particularly for HeliosPool reachability through Cloudflare and
long-runtime memory/watchdog behavior.
