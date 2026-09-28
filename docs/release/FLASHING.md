# V1.8.3-FX1 flashing guide

This is an unofficial NerdMiner_v2 modification, not an official BitMaker-hub or pool release. The supported target is **ESP32_2432S028_2USB**, classic ESP32, 4 MB flash. Do not use these images on a different board.

## Preserve an existing configuration: application-only upgrade

Use `NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-application.bin` only after confirming that the board's **actual** partition table has a compatible application partition beginning at `0x10000` and large enough for the image. On the tested layout, the sole OTA0 application partition is `0x10000`–`0x30FFFF` (size `0x300000`); NVS is at `0x9000`–`0xDFFF`, and the filesystem starts at `0x310000`.

Back up the device before updating. Do **not** run `erase_flash`, erase NVS or the filesystem, or flash the factory image when preserving Wi-Fi and mining settings. With a verified compatible layout, a typical esptool command is:

```text
python -m esptool --chip esp32 --port <YOUR_PORT> write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-application.bin
```

Verify the write, reboot normally, and confirm that the saved configuration loads. A different partition layout requires its own compatibility check; do not assume that `0x10000` is safe for every ESP32 firmware.

## Clean installation or recovery: factory image

`NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-factory.bin` is a merged image flashed at `0x0000`. It contains the classic-ESP32 bootloader at `0x1000`, partition table at `0x8000`, `boot_app0` at `0xE000`, and application at `0x10000`.

**WARNING: The factory image spans the NVS/configuration region and can erase or replace user settings. Do not use it for a configuration-preserving upgrade.**

For a deliberate clean install or recovery after preserving a full-flash backup:

```text
python -m esptool --chip esp32 --port <YOUR_PORT> write_flash 0x0000 NerdMinerV2-V1.8.3-FX1-ESP32_2432S028_2USB-factory.bin
```

For rollback, keep a byte-verified full-flash backup or known-good firmware and its compatible partition table. Restore the full backup only when its identity and size have been verified for that same board; a full restore also restores the saved configuration from the backup. Never flash a factory image merely to change the application on a configured board.
