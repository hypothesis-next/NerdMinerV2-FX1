import subprocess
import re
import hashlib
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
env.Append(LINKFLAGS=["-Wl,--wrap=" + name for name in (
    "mbedtls_ssl_handshake", "mbedtls_sha1_starts_ret", "mbedtls_sha256_starts_ret",
    "mbedtls_sha512_starts_ret", "mbedtls_x509_crt_parse", "mbedtls_ctr_drbg_seed")])

if env["PIOENV"].startswith("ESP32_2432S028_2USB"):
    # Compile the matching official TLS runtime consistently with a supported
    # smaller outgoing-record limit. Incoming records retain the full 16 KiB.
    # Never shrink a precompiled runtime's allocation behind its bounds checks.
    sdk_headers = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32")) / \
        "tools/sdk/esp32/include/mbedtls/mbedtls/include/mbedtls"
    for name, expected in {
        "ssl.h": "1d2d56b9c22e7c10754ac20cdf4bccc12a2a5380dad36e4910b1d40e40f238d1",
        "ssl_internal.h": "dfeb480a078beb370ad9439bc333742112b10edb2ada8cdce75e5731e86f6a1d",
    }.items():
        if hashlib.sha256((sdk_headers / name).read_bytes()).hexdigest() != expected:
            raise RuntimeError("TLS runtime/SDK ABI mismatch: verify pinned framework before building")
    env.Append(BUILD_FLAGS=["-DMBEDTLS_SSL_OUT_CONTENT_LEN=1024"])
    env.BuildSources("$BUILD_DIR/stats-tls-runtime", "$PROJECT_DIR/vendor/mbedtls-tls")

# Preserve useful file names in diagnostics without embedding the builder's
# private package-installation directory in release binaries.
framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if framework_dir:
    framework_prefix = str(framework_dir).replace("\\", "/")
    env.Append(BUILD_FLAGS=[
        "-ffile-prefix-map=" + framework_prefix + "=framework-arduinoespressif32"
    ])
