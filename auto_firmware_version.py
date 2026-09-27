import subprocess
import re
from pathlib import Path

Import("env")

def get_firmware_specifier_build_flag():
    version_header = Path(env.subst("$PROJECT_DIR")) / "src" / "version.h"
    match = re.search(r'#define\s+CURRENT_VERSION\s+"([^"]+)"',
                      version_header.read_text(encoding="utf-8"))
    if not match:
        raise RuntimeError("CURRENT_VERSION was not found in src/version.h")
    build_version = match.group(1)
    build_flag = "-D AUTO_VERSION=\\\"" + build_version + "\\\""
    print ("Firmware Revision: " + build_version)
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)

# Preserve useful file names in diagnostics without embedding the builder's
# private package-installation directory in release binaries.
framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    framework_prefix = str(framework_dir).replace("\\", "/")
    env.Append(BUILD_FLAGS=[
        "-ffile-prefix-map=" + framework_prefix + "=framework-arduinoespressif32"
    ])
