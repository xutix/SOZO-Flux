# SOZO

SOZO is an ESP32 spatial lighting project with an ESP32-S3 gateway, ESP32-C3 light nodes, and shared lighting/control libraries.

## Repository layout

- `SOZO-ESP32-S3/`: ESP32-S3 gateway firmware and web control page.
- `SOZO-ESP32-C3/`: ESP32-C3 light-node firmware.
- `SOZO-Common/`: shared domain, lighting, node protocol, and transport libraries used by both firmware projects.

## Build

Open either firmware folder in PlatformIO, or run from the command line:

```powershell
cd SOZO-ESP32-S3
platformio run
```

```powershell
cd SOZO-ESP32-C3
platformio run
```

## OTA password

The public source does not hard-code a private OTA password. For local OTA builds, define `SOZO_OTA_PASSWORD` in your local PlatformIO build flags, for example:

```ini
build_flags =
  -DSOZO_OTA_PASSWORD=\"your-local-password\"
```
