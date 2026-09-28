# ESP32_2432S028_2USB hardware validation checklist

> Historical multi-pool bring-up checklist. The completed V1.8.3-FX1 RC1
> measurements are in [the release validation report](release/HARDWARE_VALIDATION.md).

Use a second/development board and the merged factory binary for the first
test. Record serial logs, timestamps, dashboard observations, and resets. Do
not publish Wi-Fi credentials or wallet addresses in logs or issue reports.

## Initial flash and boot

- [ ] Confirm the artifact checksum before flashing.
- [ ] Flash the factory image at `0x0000`.
- [ ] Confirm first boot and the displayed `V1.8.3-multipool.1` version.
- [ ] Confirm correct orientation, colors, backlight, and touch/button input.
- [ ] Confirm BOOT/touch screen switching and RESET behavior.
- [ ] Confirm both lower-panel color themes and all four main screens.

## Configuration

- [ ] Open the Wi-Fi Manager configuration portal.
- [ ] Save SSID/password without exposing them in captured output.
- [ ] Save Pool URL, Pool Port, BTC username/address, optional `.worker`
  suffix, and password.
- [ ] Reboot and confirm configuration persistence.

## HeliosPool mining and dashboard

- [ ] Configure `btc.heliospool.com:3333`.
- [ ] Confirm Stratum subscribe and authorize succeed.
- [ ] Confirm `mining.notify` jobs continue arriving.
- [ ] Confirm local hashrate and block-template counters advance.
- [ ] Confirm submitted shares are accepted.
- [ ] Confirm the worker appears in the Helios web dashboard.
- [ ] Confirm the lower panel says `HeliosPool`.
- [ ] Confirm the ESP32 reaches
  `stats-btc.heliospool.com/api/users/snapshot` through Cloudflare with normal
  validated TLS.
- [ ] Compare **Best Ever** with the dashboard's user `bestEver` value.
- [ ] Compare **Workers** with the count of workers active within 24 hours.
- [ ] Compare **Total Hash Rate** with the user five-minute aggregate; allow
  normal sampling and display-rounding differences.

## Provider and failure cases

- [ ] Public Pool: verify name and all three remote metrics.
- [ ] NerdMiner Pool, Seth for Privacy, and SoloMining.de: smoke-test retained
  compatible behavior where accounts are available.
- [ ] TESTNET: verify the existing special display behavior.
- [ ] Unknown custom pool: verify hostname plus three `N/A` values and verify
  no Public Pool HTTP request occurs.
- [ ] Disconnect the statistics host while leaving Stratum reachable; hashing
  and job/share processing must continue.
- [ ] Simulate DNS failure, connection refusal, TLS failure, timeout, HTTP 404,
  429, 500/503, malformed JSON, missing fields, and an oversized response.
- [ ] Confirm no tight retry loop; observe 15-300 second failure backoff.
- [ ] Confirm last-good values become `STALE` and no-cache values become
  `ERROR`/`N/A`.
- [ ] Restore the API and confirm automatic recovery without a Stratum
  reconnect.

## Connectivity and endurance

- [ ] Disconnect/reconnect Wi-Fi and confirm both Wi-Fi and Stratum recover.
- [ ] Interrupt Stratum only and confirm normal Stratum reconnection.
- [ ] Confirm statistics failures never cause Stratum reconnection.
- [ ] Run at least 24 hours, preferably 72 hours.
- [ ] Record free heap, minimum free heap, largest free block, and reset reason
  periodically from a diagnostic build or debugger.
- [ ] Check the statistics task stack high-water mark with a diagnostic build.
- [ ] Confirm no monotonic heap loss, watchdog resets, boot loops, or degraded
  share acceptance.

Do not declare the release hardware-validated until every critical mining,
TLS, failure-isolation, and endurance item has evidence from the physical board.
