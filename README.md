# Leafy IoT ESP32 Firmware

PlatformIO firmware for an ESP32 or ESP32-CAM Leafy IoT module. The firmware follows the current `iot-metrics-collector-service` v1 contract and keeps the device code modular under `src/app`, `src/models`, and `src/utils`.

Camera capture is collector-triggered. The firmware does not capture on a local timer; it waits for a collector MQTT command, captures a JPEG on ESP32-CAM builds, uploads the bytes through file-service HTTP multipart upload, then publishes result metadata back over MQTT. The collector may send `triggerType` as `MANUAL` or `SCHEDULED`; both use the same queued capture/upload path.

## Backend Contract

- MQTT namespace defaults to `coffee/prod`.
- Status publishes to `coffee/prod/devices/{deviceUid}/status`.
- Telemetry publishes to `coffee/prod/devices/{deviceUid}/telemetry`.
- Config is received from `coffee/prod/devices/{deviceUid}/config/set`.
- Config ACK publishes to `coffee/prod/devices/{deviceUid}/ack`.
- Config ACK payload uses `type=config`.
- Camera capture commands are received from `coffee/prod/devices/{deviceUid}/camera/capture`.
- Camera result metadata publishes to `coffee/prod/devices/{deviceUid}/image/meta`.
- MQTT never carries image bytes. It carries capture commands, file metadata, and failure status only.

Telemetry payload fields remain:

- `ts`
- `firmwareVersion`
- `battery`
- `rssi`
- `metrics`

Telemetry metric codes are fixed to backend-recognized values:

| Sensor | Metric code | Firmware value |
| --- | --- | --- |
| DHT11 temperature | `AIR_TEMP` | Celsius |
| DHT11 humidity | `AIR_HUMIDITY` | Percent RH |
| YL-69 soil sensor | `SOIL_MOISTURE` | Normalized 0..100 percent |
| LDR/photoresistor | `LIGHT_INTENSITY` | Normalized 0..1000 brightness scale, not true lux |

Do not add or rename metric codes unless the backend `sensor_types.code` data is updated first.

## Implemented Modules

- `config_store`: NVS-backed factory identity/MQTT reads plus runtime Wi-Fi, topic routing, config, and calibration load/save.
- `wifi_manager`: Wi-Fi station connect/reconnect with backoff and setup-mode placeholder.
- `mqtt_manager`: MQTT connect/reconnect, config subscription, status, telemetry, ACK, and topic builders.
- `status_service`: backend-compatible online heartbeat.
- `config_service`: config parse, validation, version handling, runtime apply, persistence verification, rollback attempt, and ACK.
- `camera_service`: ESP32-CAM-only camera init, JPEG capture, metadata, and frame release.
- `file_upload_service`: HTTP multipart upload to file-service `POST /files/upload`.
- `sensor_manager`: DHT11, YL-69, and LDR reads with per-sensor validity, normalization, and optional calibration logs.
- `telemetry_service`: sampling and publishing based on runtime config, omits invalid metrics, skips empty payloads.
- `setup_portal`: SoftAP HTTP portal for local Wi-Fi setup, device info, diagnostics, and runtime reset.
- `device_runtime`: explicit boot/setup/Wi-Fi/MQTT/ready/running/config/recovery state machine.

## Hardware Assumptions

Expected prototype stack:

- ESP32 devkit or ESP32-CAM.
- DHT11 data pin for air temperature and humidity.
- YL-69 analog output for soil moisture.
- Optional GPIO powering the YL-69 probe only during measurement.
- LDR/photoresistor analog divider for ambient light.

Default pins and calibration constants are in [platformio.ini](platformio.ini):

| Macro | Default | Purpose |
| --- | --- | --- |
| `LEAFY_DHT_PIN` | `13` | DHT11 data pin |
| `LEAFY_I2C_SDA_PIN` | `15` | ADS1115 I2C SDA |
| `LEAFY_I2C_SCL_PIN` | `14` | ADS1115 I2C SCL |
| `LEAFY_SOIL_ADS_CHANNEL` | `1` | YL-69 ADS1115 input |
| `LEAFY_LDR_ADS_CHANNEL` | `0` | LDR ADS1115 input |
| `LEAFY_RUNTIME_RESET_BUTTON_PIN` | `2` | Runtime reset long-press GPIO, wired to GND through a normally-open button |
| `LEAFY_RUNTIME_RESET_HOLD_MS` | `5000` | Hold time required before clearing `leafy_runtime` |
| `LEAFY_RUNTIME_RESET_DEBOUNCE_MS` | `50` | Debounce window for the runtime reset button |
| `LEAFY_ANALOG_MAX` | `4095` | 12-bit ESP32 ADC max |

Check every pin against the real board before flashing. ESP32-CAM boards have constrained pins, and some pins are used by the camera, SD card, flash LED, or boot strapping.

## Build And Flash

Install PlatformIO, then build from this directory:

```bash
pio run -e esp32dev
```

Flash and monitor:

```bash
pio run -e esp32dev -t upload
pio device monitor
```

For ESP32-CAM:

```bash
pio run -e esp32cam
pio run -e esp32cam -t upload
```

The `esp32cam` environment defines `LEAFY_ESP32_CAM=1`. Camera capture code compiles and runs only with that flag; the `esp32dev` environment still builds without requiring camera hardware.

## On-Demand Camera Capture

Capture flow:

1. The web device detail page calls `POST /iot/devices/{deviceId}/camera/capture`.
2. The collector creates a media event and publishes `coffee/{env}/devices/{deviceUid}/camera/capture`.
3. ESP32-CAM receives the command, captures one JPEG, and uploads it to file-service `POST /files/upload` as multipart field `file`.
4. File-service stores the image through its existing S3 flow and returns a file id.
5. ESP32-CAM publishes `coffee/{env}/devices/{deviceUid}/image/meta` with `requestId`, `success`, `fileId`, `contentType`, `sizeBytes`, `width`, and `height`.
6. The collector updates the matching `DeviceMediaEvent`; the frontend refetches/polls `/iot/devices/{deviceId}/media`.

The collector command uses the file-service upload model that exists today:

```json
{
  "requestId": "uuid",
  "deviceUid": "leafy-prototype-001",
  "triggerType": "MANUAL",
  "requestedAt": "2026-04-25T10:00:00Z",
  "resolution": "VGA",
  "quality": "MEDIUM",
  "upload": {
    "mode": "FILE_SERVICE_MULTIPART",
    "endpoint": "http://192.168.1.10:8084/internal/files/upload"
  }
}
```

Scheduled captures use the same payload shape, with only `triggerType` changed:

```json
{
  "requestId": "uuid",
  "deviceUid": "leafy-prototype-001",
  "triggerType": "SCHEDULED",
  "requestedAt": "2026-05-15T08:00:00Z",
  "resolution": "VGA",
  "quality": "MEDIUM",
  "upload": {
    "mode": "FILE_SERVICE_MULTIPART",
    "endpoint": "http://192.168.1.10:8084/internal/files/upload"
  }
}
```

Set collector property `app.file-service.upload-url` to a URL reachable from the ESP32-CAM, not just from the backend container. Physical devices must not receive `localhost`, `127.0.0.1`, or `0.0.0.0` because those addresses point back to the ESP32-CAM itself. For local LAN testing, use the backend machine IP and the no-auth internal file-service upload endpoint, for example:

```properties
FILE_SERVICE_UPLOAD_URL=http://192.168.1.10:8084/internal/files/upload
```

If you route uploads through the API gateway instead, use the gateway's LAN IP/port and a path that the device is allowed to call. The firmware reads only `upload.endpoint` from the MQTT capture command at runtime; it no longer falls back to a compiled localhost URL.

Expected capture logs:

```text
[INFO] Subscribed camera capture topic coffee/prod/devices/{deviceUid}/camera/capture
[INFO] Received camera capture command on coffee/prod/devices/{deviceUid}/camera/capture, bytes=...
[INFO] Queued SCHEDULED camera capture requestId=...
[INFO] Scheduled capture command accepted for deviceUid={deviceUid}
```

Failure behavior:

- Invalid command, missing `requestId`, missing upload endpoint, camera init/capture failure, and upload failure publish `success=false` to `image/meta`.
- Capture work is queued from the MQTT callback and retried up to 3 attempts with a 5 second delay before publishing final failure metadata.
- Capture failures do not reboot the device.
- Non-camera `esp32dev` builds publish `CAMERA_NOT_ENABLED` if a capture command is received.
- The firmware starts with VGA/MEDIUM defaults; QVGA can be requested to reduce memory and upload size.

Bench overrides:

- Set `LEAFY_DEFAULT_DEVICE_UID` and `LEAFY_DEFAULT_DEVICE_CODE` to match the backend provisioned record.
- Set `LEAFY_DEFAULT_MQTT_HOST` to the broker reachable from the device.
- If you want to skip setup portal during bench testing, use NVS-preloaded Wi-Fi credentials or temporary local build flags for `LEAFY_DEFAULT_WIFI_SSID` and `LEAFY_DEFAULT_WIFI_PASSWORD`.
- Do not commit real Wi-Fi passwords or broker credentials.

## NVS Storage Policy

The firmware separates factory provisioning from runtime configuration:

| Namespace | Keys | Reset behavior |
| --- | --- | --- |
| `leafy_factory` | `deviceUid`, `deviceCode`, `deviceType`, `mqttHost`, `mqttPort`, `mqttUser`, `mqttPass` | Read-only during normal firmware runtime. Setup portal reset and runtime long-press reset preserve this namespace. |
| `leafy_runtime` | `wifiSsid`, `wifiPass`, `mqttProduct`, `mqttEnv`, `sampleSec`, `publishSec`, `offlineSec`, `alertEnabled`, `cfgVersion`, `soilDryRaw`, `soilWetRaw`, `lightDarkRaw`, `lightBrightRaw`, schedule state keys such as `enabled`, `timeOfDay`, `recurrence`, `triggerType`, `lastRunAt`, `nextRunAt` | Mutable and resettable. Runtime reset clears this namespace only. |

Factory values are loaded from `leafy_factory` when present, otherwise from flash-time `LEAFY_DEFAULT_*` build flags. The normal firmware API intentionally has no `saveIdentity()` or `saveMqttConfig()` path, so production identity and broker credentials cannot be overwritten by setup portal or MQTT config messages.

`mqttProduct` and `mqttEnv` remain runtime-side topic routing overrides. Treat them as semi-immutable in production because changing them also changes every MQTT topic the collector must subscribe to.

## Local Setup Portal V1

The local portal is for device-side setup and diagnostics only. It does not provision the device, generate claim codes, claim the device, bind a farm plot/zone, or store user tokens. Those actions remain in the backend/web/mobile flow.

Setup mode is entered when required local setup is missing, especially when Wi-Fi SSID is absent. A future physical button trigger can call the same `WIFI_SETUP_MODE` path.

In setup mode:

- Normal Wi-Fi station and MQTT runtime are suspended.
- The device starts SoftAP mode.
- The AP SSID uses this pattern: `Leafy-Setup-<suffix>`.
- The suffix comes from the end of `deviceUid`, or from chip ID if identity is missing.
- The AP is open for prototype bring-up.
- The portal runs at `http://192.168.4.1`.

Serial logs look like:

```text
[WARN] Required local setup is missing; Wi-Fi setup mode required
[WARN] Entering Wi-Fi setup mode
[INFO] Setup AP started: SSID=Leafy-Setup-YPE001, IP=192.168.4.1
[INFO] Captive portal DNS started on 192.168.4.1:53
[INFO] Setup portal started: http://192.168.4.1
```

Portal pages:

| Route | Purpose |
| --- | --- |
| `GET /` | Device info plus a Leafy onboarding QR payload, text JSON fallback, runtime state, Wi-Fi state, AP SSID, portal URL, MQTT state |
| `GET /device-info` | Versioned JSON identity payload for app/web onboarding QR scans |
| `GET /qr-code.js` | Local offline QR renderer used by the setup portal |
| `GET /generate_204`, `/gen_204` | Android captive portal probes; redirect to the setup portal |
| `GET /hotspot-detect.html`, `/library/test/success.html` | Apple captive portal probes; redirect to the setup portal |
| `GET /connecttest.txt`, `/ncsi.txt`, `/fwlink`, `/canonical.html` | Windows/browser captive portal probes; redirect to the setup portal |
| `GET /wifi` | Wi-Fi SSID/password form |
| `POST /wifi` | Validate and save Wi-Fi credentials, then reboot |
| `GET /diagnostics` | Runtime config, calibration, Wi-Fi/MQTT state, heap, uptime, and current sensor readings |
| `GET /reset` | Confirmation page for clearing runtime config |
| `POST /reset` | Clear `leafy_runtime`, preserve `leafy_factory`, then reboot |
| `GET /api/status` | Tiny JSON status endpoint for quick local checks |

Captive portal auto-open:

- In setup mode, the firmware starts a local DNS server on port `53`.
- DNS wildcard `*` resolves captive-check hostnames to the setup AP IP.
- Common Android, Apple, and Windows captive portal probe routes redirect to `http://192.168.4.1/`.
- Unknown browser routes also redirect to the setup portal, so URLs such as `http://example.com` should land on the Leafy portal while connected to the setup AP.
- Unknown `/api/*` routes return `{"error":"not_found"}` with HTTP `404` so API mistakes are still visible.
- Auto-open is best-effort. Some operating systems cache captive state, require tapping a Wi-Fi sign-in notification, or are affected by VPN/mobile data settings.
- Manual fallback remains `http://192.168.4.1`.

Setup portal device QR:

- The portal root renders a QR code titled `Connect this device to Leafy`.
- The QR encodes the same compact JSON returned by `GET /device-info`.
- The page also shows a read-only JSON textarea fallback for copy/paste if QR scanning is unavailable.
- The QR is generated locally by `/qr-code.js`; it does not load scripts from the internet.

`GET /device-info` response shape:

```json
{
  "type": "LEAFY_IOT_DEVICE",
  "version": 1,
  "deviceUid": "leafy-prototype-001",
  "deviceCode": "LEAFY-PROTO-001",
  "deviceType": "ESP32",
  "model": "Leafy IoT Module V1",
  "firmwareVersion": "leafy-esp32-0.1.0",
  "setupApSsid": "Leafy-Setup-YPE001",
  "setupPortalUrl": "http://192.168.4.1"
}
```

Mobile/web onboarding should scan this payload, read `deviceUid`, `deviceCode`, and `deviceType`, then ask the user to select a farm plot, zone, and display name before calling backend `POST /iot/devices/connect`.

Security notes:

- The QR and `/device-info` endpoint do not include `wifiPass`, `mqttUser`, `mqttPass`, `mqttHost`, or `mqttPort`.
- Wi-Fi credentials remain local to `/wifi` and are saved only in the `leafy_runtime` namespace.
- MQTT broker credentials remain factory/runtime internals and are not rendered in the setup portal QR payload.

Wi-Fi save behavior:

1. Connect to the setup AP.
2. Open `http://192.168.4.1/wifi`.
3. Enter SSID and password.
4. Submit the form.
5. The firmware saves credentials with `config_store`, does not log the password, and reboots after a short delay.
6. On reboot, the normal state machine attempts station Wi-Fi, MQTT, status, telemetry, and config subscription.

Reset behavior:

- `POST /reset` clears only the `leafy_runtime` namespace.
- Wi-Fi credentials, runtime config, topic routing overrides, calibration, and any firmware-side schedule state are cleared.
- Device identity and MQTT broker credentials in `leafy_factory` remain intact.
- The device reboots back into setup mode because Wi-Fi credentials are gone.

Physical reset behavior:

- The ESP32-CAM-MB `RST`/`EN` button is a hardware reset and cannot be detected by firmware.
- GPIO2 is configured as `INPUT_PULLUP` for runtime reset. Wire a normally-open button from GPIO2 to GND.
- Holding GPIO2 low for at least `LEAFY_RUNTIME_RESET_HOLD_MS` clears only `leafy_runtime`; the firmware does not restart and active Wi-Fi/MQTT tasks keep running.
- Short presses are ignored after debounce. A successful clear prints `Long press detected, runtime NVS cleared` on serial.
- Do not use GPIO0 for this on AI-Thinker ESP32-CAM while the camera is active; GPIO0 is used by the camera clock/boot strapping.
- GPIO2 is a boot strapping pin and may also be used by the SD card interface on some ESP32-CAM setups. Keep it released/high during boot and do not enable SD card features while it is used as the runtime reset button.

## Bring-Up Sequence

1. Wire sensors and power, then verify the board can boot from USB power.
2. Check `platformio.ini` identity, broker host, pins, and calibration defaults.
3. Flash firmware.
4. Open serial monitor at `115200`.
5. Confirm boot logs:

```text
[INFO] Leafy IoT firmware starting
[INFO] Runtime begin
[INFO] State BOOT -> LOAD_LOCAL_CONFIG
```

6. Confirm local config load:

```text
[INFO] Loaded local config: deviceUid=..., wifiConfigured=true, mqtt=...
[INFO] Sensor calibration active: soilDryRaw=..., soilWetRaw=..., lightDarkRaw=..., lightBrightRaw=...
```

7. Confirm Wi-Fi:

```text
[INFO] Connecting Wi-Fi SSID=...
[INFO] Wi-Fi connected: 192.168.x.x
```

8. Confirm MQTT:

```text
[INFO] Connecting MQTT broker host:1883
[INFO] MQTT connected
[INFO] Subscribed config topic coffee/prod/devices/{deviceUid}/config/set
[INFO] Subscribed camera capture topic coffee/prod/devices/{deviceUid}/camera/capture
```

9. Confirm running state:

```text
[INFO] State READY -> RUNNING
[INFO] Status published online=true
[INFO] Telemetry sample captured: validMetrics=4/4
[INFO] Telemetry published: validMetrics=4, rssi=-61
```

10. Push config from the backend and confirm:

```text
[INFO] Received config payload on coffee/prod/devices/{deviceUid}/config/set, bytes=...
[INFO] State RUNNING -> APPLYING_CONFIG
[INFO] Runtime config is active: version=2, sampleSec=30, publishSec=120, offlineSec=900, alertEnabled=true, statusHeartbeatSec=30
[INFO] Saved and verified runtime config version 2
[INFO] Config ACK published: version=2, success=true
[INFO] State APPLYING_CONFIG -> RUNNING
```

If Wi-Fi config is missing, validate the local setup portal before normal runtime:

```text
[WARN] Required local setup is missing; Wi-Fi setup mode required
[INFO] Setup AP started: SSID=Leafy-Setup-..., IP=192.168.4.1
[INFO] Setup portal started: http://192.168.4.1
```

Connect a phone or laptop to the setup AP, open `http://192.168.4.1`, save Wi-Fi credentials at `/wifi`, and wait for the device to reboot into station mode.

## Telemetry Scheduling

Runtime config controls telemetry:

- `samplingIntervalSec`: sensor sampling cadence.
- `publishIntervalSec`: MQTT telemetry publish cadence.
- `configVersion`: version stored with config ACK.

The firmware samples sensors at `samplingIntervalSec`, stores the latest sample that contains at least one valid metric, and publishes a new valid sample at `publishIntervalSec`. Invalid readings are omitted from `metrics`. If no new valid sample exists, telemetry publish is skipped for that interval. The firmware does not publish fake zeros.

`alertEnabled` is stored and ACKed but does not suppress telemetry in this phase. Backend alert evaluation remains server-side.

## Config Apply Behavior

Accepted config fields:

- `samplingIntervalSec`
- `publishIntervalSec`
- `offlineTimeoutSec`
- `alertEnabled`
- `configVersion`

Validation rules:

- `samplingIntervalSec > 0`
- `publishIntervalSec > 0`
- `offlineTimeoutSec > 0`
- `publishIntervalSec >= samplingIntervalSec`
- `offlineTimeoutSec > publishIntervalSec`
- `configVersion >= 1`

Version behavior:

- Older `configVersion`: ignored and ACKed as failure with `stale config version`.
- Same `configVersion` with identical values: idempotent re-apply, ACK success.
- Same `configVersion` with different values: rejected, ACK failure.
- Newer `configVersion`: applied to live runtime, persisted to NVS, then ACK success.

ACK success means the config is effective in runtime and saved. If persistence fails after live apply, the firmware attempts to restore the previous NVS value and roll runtime behavior back before ACKing failure.

`samplingIntervalSec` and `publishIntervalSec` take effect immediately. `offlineTimeoutSec` is stored and logged, but the v1 firmware keeps status heartbeat fixed at `DEFAULT_STATUS_HEARTBEAT_SEC` because the backend contract has no heartbeat interval field.

## Calibration Workflow

Calibration values:

| Macro / NVS key | Default | Meaning |
| --- | --- | --- |
| `LEAFY_SOIL_DRY_RAW` / `soilDryRaw` | `3200` | Raw ADC reading for dry soil |
| `LEAFY_SOIL_WET_RAW` / `soilWetRaw` | `1200` | Raw ADC reading for wet soil |
| `LEAFY_LIGHT_DARK_RAW` / `lightDarkRaw` | `3500` | Raw ADC reading in dark condition |
| `LEAFY_LIGHT_BRIGHT_RAW` / `lightBrightRaw` | `500` | Raw ADC reading in bright condition |

To log raw ADC samples during bench calibration, set:

```ini
-D LEAFY_CALIBRATION_LOGGING=1
-D LEAFY_CALIBRATION_LOG_INTERVAL_SEC=10
```

Then flash and watch logs like:

```text
[INFO] Calibration sample: soilRaw=3180, soilPct=1.0, lightRaw=3440, lightNorm=20.7, soilCal=3200/1200, lightCal=3500/500
```

Soil calibration:

1. Keep the probe clean and dry in air or known dry soil.
2. Record several `soilRaw` values and average them. Use that as `soilDryRaw`.
3. Place the probe in fully wet reference soil or water-saturated soil.
4. Record several `soilRaw` values and average them. Use that as `soilWetRaw`.
5. Set the macros or store the NVS calibration values.
6. Reflash or reload config, then verify dry reads near 0 and wet reads near 100.

Light calibration:

1. Cover the LDR or place it in the darkest expected enclosure condition.
2. Record several `lightRaw` values and average them. Use that as `lightDarkRaw`.
3. Place the LDR in the brightest expected condition.
4. Record several `lightRaw` values and average them. Use that as `lightBrightRaw`.
5. Reflash or reload config, then verify dark reads near 0 and bright reads near 1000.

The LDR output is a normalized brightness scale, not lux.

## Backend-Side Validation

Use this before moving to setup portal or camera work:

1. Start the IoT backend stack and MQTT broker.
2. Provision the device through the backend/web flow with the same `deviceUid`, `deviceCode`, `deviceName`, and `deviceType` that firmware uses.
3. Generate a claim code.
4. Claim/bind the device to a `farmPlotId` and `zoneId`.
5. Confirm `sensor_types` contains `AIR_TEMP`, `AIR_HUMIDITY`, `SOIL_MOISTURE`, and `LIGHT_INTENSITY`.
6. Flash firmware and open serial monitor.
7. Confirm Wi-Fi and MQTT connection logs.
8. Confirm status reaches `coffee/prod/devices/{deviceUid}/status` and the collector marks the device `ONLINE`.
9. Confirm telemetry reaches `coffee/prod/devices/{deviceUid}/telemetry`.
10. Query latest readings from the collector and confirm metrics appear under the expected zone.
11. Push a newer config with changed `samplingIntervalSec` and `publishIntervalSec`.
12. Confirm serial logs show config receipt, validation, runtime apply, NVS save, and ACK publish.
13. Confirm ACK reaches `coffee/prod/devices/{deviceUid}/ack` and backend accepts the matching `configVersion`.
14. Confirm the new sampling/publish intervals take effect without rebooting.
15. Push an invalid config such as `publishIntervalSec < samplingIntervalSec`; confirm ACK failure and previous active config remains effective.
16. Push an older `configVersion`; confirm it is ignored as stale.
17. On ESP32-CAM, press "Chụp ảnh hiện tại" on the device detail page.
18. Confirm the serial log receives one capture command, uploads to file-service, and publishes one `image/meta` result.
19. Confirm `/iot/devices/{deviceId}/media` shows `UPLOADED` with a file id, or `FAILED` with an error if upload/capture failed.

## Troubleshooting

No Wi-Fi connection:

- Confirm `wifiConfigured=true` in boot logs.
- Confirm SSID spelling and that the network is 2.4 GHz.
- Check signal strength and power stability.
- If credentials are absent, firmware starts `Leafy-Setup-<suffix>` and serves the local portal at `http://192.168.4.1`.
- If credentials were saved incorrectly, connect to the setup portal after clearing Wi-Fi config with `/reset`, or clear NVS during flashing.

MQTT not connecting:

- Confirm `LEAFY_DEFAULT_MQTT_HOST` or NVS `mqttHost` is reachable from the Wi-Fi network.
- Confirm broker port `1883` is open.
- Confirm broker accepts unauthenticated clients or the configured username/password.
- Check MQTT return code in serial logs.

Device not becoming ONLINE:

- Telemetry alone does not mark the backend device online.
- Confirm status publishes to `coffee/prod/devices/{deviceUid}/status`.
- Confirm device UID in topic exactly matches the provisioned backend record.

Telemetry not appearing:

- Confirm device is active, claimed, and bound to a zone.
- Confirm all metric codes exist in `sensor_types`.
- Confirm serial log reports at least one valid metric.
- Confirm MQTT telemetry topic is `coffee/prod/devices/{deviceUid}/telemetry`.

Config ACK not accepted:

- Confirm ACK topic is `coffee/prod/devices/{deviceUid}/ack`.
- Confirm ACK `type` is `config`.
- Confirm ACK `configVersion` matches the backend current config version.
- Confirm the config was not rejected by firmware validation.

Sensor readings invalid:

- Check sensor wiring and power.
- Check DHT11 data pin and pull-up wiring.
- Confirm ADC pins are valid for the target ESP32 board.
- Enable `LEAFY_CALIBRATION_LOGGING=1` to inspect raw soil and light ADC values.

YL-69 unstable or noisy:

- Use short wiring and stable 3.3 V power.
- Prefer power-on-demand through `LEAFY_SOIL_POWER_PIN`.
- Average repeated dry/wet readings before choosing calibration values.
- Remember YL-69 probes corrode quickly if powered continuously.

LDR range looks wrong:

- Confirm the voltage divider direction. Some circuits produce lower raw values when brighter; the defaults assume that direction.
- Recalibrate in the actual enclosure and placement.
- Do not treat `LIGHT_INTENSITY` as lux.

## Focused Docs

- [Hardware validation checklist](../docs/iot-device/iot-device-hardware-validation-checklist.md)
- [Calibration guide](../docs/iot-device/iot-device-calibration-guide.md)
- [Protocol v1](../docs/iot-device/iot-device-protocol-v1.md)

## Known Limitations

- Local setup portal v1 has no captive DNS and no rich UI.
- Local setup portal v1 saves Wi-Fi credentials only; MQTT endpoint and calibration editing remain build/NVS tasks.
- No durable telemetry queue exists yet.
- Camera capture supports on-demand JPEG capture only; no periodic capture and no video.
- File upload uses file-service multipart upload because file-service does not currently expose a device-safe presigned PUT creation endpoint.
- DHT11 is low precision and slow.
- YL-69 corrosion risk remains a hardware concern.
- LDR output is normalized brightness, not true lux.
- Production MQTT authentication is not solved by the current backend contract.

## Next Phase

Recommended next work:

1. Add optional MQTT endpoint editing to the local setup portal.
2. Add an NVS-backed calibration update path through the local setup portal.
3. Add NTP setup if device-side timestamps are required.
4. Add camera/image flow only after the backend media pipeline is defined.
