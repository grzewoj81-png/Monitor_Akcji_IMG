---
description: "Use when changing ESP32 firmware, C++ source, or PlatformIO configuration in this project. Build and automatically upload the firmware to the connected board."
applyTo: "src/**/*.cpp, src/**/*.h, platformio.ini"
---
# PlatformIO Firmware Upload

- After changing firmware source or PlatformIO configuration, build the `esp32-2432s028` environment and, if the build succeeds, automatically upload it to the connected board.
- Do not upload if the build fails. If no compatible board is connected or upload fails, report that clearly; do not treat a successful build as a successful upload.
- Documentation-only changes do not require a firmware upload.
