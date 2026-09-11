#!/usr/bin/env python3
"""
Post-build script for NerdMiner_v2 firmware merging
Ported from merge_firmware_universal.js - detects ESP32 type from bootloader signature
Generates factory (0x0) and update (0x10000) files automatically after build

Usage: Add to platformio.ini environments:
extra_scripts =
    pre:auto_firmware_version.py
    post:post_build_merge.py
"""

import os
import subprocess
import re
from pathlib import Path

Import("env")

def detect_esp32_type(bootloader_path):
    """Detect ESP32 type by analyzing bootloader signature - ported from JS version"""
    try:
        with open(bootloader_path, 'rb') as f:
            bootloader_data = f.read()
        
        if len(bootloader_data) < 13:
            print("Warning: Bootloader too small, defaulting to ESP32")
            return 'ESP32'
            
        chip_id = bootloader_data[12]  # Chip ID at byte 12
        size = len(bootloader_data)
        
        # Detect ESP32 type based on bootloader signature (from JS version)
        if chip_id == 0x09 and size >= 15000:
            return 'ESP32-S3'
        elif chip_id == 0x05 and size >= 13000 and size < 14000:
            return 'ESP32-C3'
        elif chip_id == 0x02 and size >= 13000 and size < 15000:
            return 'ESP32-S2'
        elif chip_id == 0x00 and size >= 17000:
            return 'ESP32'
        else:
            # Fallback: try to detect by size (from JS version)
            if size >= 17000:
                return 'ESP32'
            elif size >= 15000:
                return 'ESP32-S3'
            elif size >= 13600:
                return 'ESP32-S2'
            elif size >= 13000:
                return 'ESP32-C3'
            else:
                return 'ESP32'  # Default fallback
                
    except Exception as e:
        print(f"Warning: Could not analyze bootloader ({e}), defaulting to ESP32")
        return 'ESP32'

def get_memory_layout(esp_type):
    """Get memory addresses for each ESP32 variant - ported from JS version"""
    if esp_type == 'ESP32-C3':
        # ESP32-C3: Bootloader at 0x0000, no boot_app0
        return {
            'bootloader': 0x0000,
            'partitions': 0x8000,
            'firmware': 0x10000
        }
    elif esp_type == 'ESP32-S2':
        # ESP32-S2: Bootloader at 0x1000, boot_app0 at 0xE000
        return {
            'bootloader': 0x1000,
            'partitions': 0x8000,
            'boot_app0': 0xE000,
            'firmware': 0x10000
        }
    elif esp_type == 'ESP32-S3':
        # ESP32-S3: Bootloader at 0x0000, no boot_app0
        return {
            'bootloader': 0x0000,
            'partitions': 0x8000,
            'firmware': 0x10000
        }
    else:
        # ESP32 Classic: Bootloader at 0x1000, boot_app0 at 0xE000
        return {
            'bootloader': 0x1000,
            'partitions': 0x8000,
            'boot_app0': 0xE000,
            'firmware': 0x10000
        }

def get_firmware_version():
    """Get the release identifier used by the firmware itself."""
    try:
        version_header = Path(env.subst("$PROJECT_DIR")) / "src" / "version.h"
        match = re.search(r'#define\s+CURRENT_VERSION\s+"([^"]+)"',
                          version_header.read_text(encoding="utf-8"))
        if match:
            return match.group(1)
    except:
        pass
    return "dev"

def create_merged_firmware(source, target, env):
    """Main function called after firmware build"""
    
    # Get build info
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_dir = Path(env.subst("$BUILD_DIR"))
    env_name = env.subst("$PIOENV")
    version = get_firmware_version()
    
    print(f"\n🔨 Building firmware files for {env_name}...")
    
    # File paths in build directory
    bootloader_file = build_dir / "bootloader.bin"
    partitions_file = build_dir / "partitions.bin"
    boot_app0_file = build_dir / "boot_app0.bin"
    if not boot_app0_file.exists():
        framework_dir = env.PioPlatform().get_package_dir(
            "framework-arduinoespressif32"
        )
        if framework_dir:
            framework_boot_app0 = (
                Path(framework_dir) / "tools" / "partitions" / "boot_app0.bin"
            )
            if framework_boot_app0.exists():
                boot_app0_file = framework_boot_app0
    firmware_file = build_dir / "firmware.bin"
    
    # Check if firmware exists
    if not firmware_file.exists():
        print(f"❌ Firmware file not found: {firmware_file}")
        return
    
    # Auto-detect ESP32 type
    esp_type = detect_esp32_type(bootloader_file) if bootloader_file.exists() else 'ESP32'
    addresses = get_memory_layout(esp_type)
    
    print(f"📱 Detected: {esp_type} (bootloader at 0x{addresses['bootloader']:04X})")
    
    # Output directory with version subfolder
    version_dir = project_dir / "firmware" / version
    version_dir.mkdir(parents=True, exist_ok=True)
    
    artifact_prefix = f"NerdMinerV2-{version}-{env_name}"
    factory_file = version_dir / f"{artifact_prefix}-factory.bin"
    update_file = version_dir / f"{artifact_prefix}-firmware.bin"
    
    # 1. Create update file (just copy firmware.bin)
    try:
        import shutil
        shutil.copy2(firmware_file, update_file)
        print(f"✅ Firmware: {update_file.name}")
    except Exception as e:
        print(f"❌ Error creating firmware file: {e}")
        return
    
    # 2. Create factory file (merged)
    try:
        # Create merged binary - 4MB filled with 0xFF
        merged_size = 0x400000  # 4MB
        merged_data = bytearray([0xFF] * merged_size)
        max_address = 0
        
        # Files to merge
        files_to_merge = {
            'bootloader': bootloader_file,
            'partitions': partitions_file,
            'firmware': firmware_file
        }
        
        # Add boot_app0 for ESP32 Classic and S2
        if 'boot_app0' in addresses:
            files_to_merge['boot_app0'] = boot_app0_file

        missing = [str(path) for path in files_to_merge.values() if not path.exists()]
        if missing:
            raise RuntimeError("Required flash component is missing: " + ", ".join(missing))

        occupied_ranges = []
        
        # Merge files at their respective addresses
        for file_type, file_path in files_to_merge.items():
            address = addresses[file_type]
            with open(file_path, 'rb') as f:
                data = f.read()
            end_address = address + len(data)

            print(f"   📄 {file_type} at 0x{address:06X}: {len(data)} bytes")
            if end_address > merged_size:
                raise RuntimeError(f"{file_type} exceeds the 4 MB flash image")
            for existing_type, existing_start, existing_end in occupied_ranges:
                if address < existing_end and end_address > existing_start:
                    raise RuntimeError(
                        f"{file_type} overlaps {existing_type} in the flash image"
                    )

            merged_data[address:end_address] = data
            occupied_ranges.append((file_type, address, end_address))
            max_address = max(max_address, end_address)
        
        # Find actual end of data (round up to 4K boundary)
        actual_end = ((max_address + 4095) // 4096) * 4096
        
        # Write factory file
        with open(factory_file, 'wb') as f:
            f.write(merged_data[:actual_end])
        
        print(f"✅ Factory: {factory_file.name} ({actual_end} bytes)")
        
    except Exception as e:
        print(f"❌ Error creating factory file: {e}")
        raise

# Add post-build hook
env.AddPostAction("$BUILD_DIR/firmware.bin", create_merged_firmware)
