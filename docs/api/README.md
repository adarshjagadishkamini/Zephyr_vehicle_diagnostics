# API Documentation

## CAN Protocol
All CAN messages use standard frame format.

### Message IDs
- 0x51: Battery Data
  - Bytes 0-3: Voltage (float)
- 0x52: Collision Data
  - Bytes 0-1: Distance (uint16_t)
- 0x53: Temperature Data
  - Bytes 0-3: Temperature (float)
- 0x54: GPS Data
  - Bytes 0-3: Latitude (float)
  - Bytes 4-7: Longitude (float)
- 0x55: Brake Data
  - Bytes 0-1: Pressure (uint16_t)
- 0x56: TPMS Data
  - Byte 0: Pressure (uint8_t)
- 0x57: Speed Data
  - Bytes 0-1: Speed km/h (uint16_t)

## MQTT Topics
- /topic/battery
- /topic/collision
- /topic/temperature
- /topic/gps
- /topic/speed
- /topic/v2x
- /topic/traffic_update
- /topic/hazard_notification
- /topic/predictive_maintenance
- /topic/v2i

## BLE Services
UUID: 00FF - Vehicle Configuration Service
Characteristics:
- 00F1: WIFI_SSID (Write)
- 00F2: WIFI_PASSWORD (Write)
