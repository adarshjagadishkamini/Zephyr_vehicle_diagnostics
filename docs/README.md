# Smart Vehicle Tracking System Documentation

## Overview
This document provides comprehensive documentation for the Smart Vehicle Tracking System implemented using Zephyr RTOS on ESP32-S3 platform.

## Getting Started

### Prerequisites
- Zephyr RTOS development environment
- ESP32-S3 toolchain
- Required sensors:
  - DS18B20 temperature sensor
  - NEO-6M GPS module
  - HC-SR04 ultrasonic sensor
  - INA219 current/voltage sensor
  - SP370 pressure sensor
  - Hall effect sensor
  - MPX5700DP pressure sensor

### Building
```bash
# Clone the repository
git clone https://github.com/adarshjagadishkamini/Zephyr_vehicle_diagnostics.git

# Initialize west workspace
west init -l .
west update

# Build all nodes
./build.sh
```

### Flashing
```bash
# Flash individual nodes
west flash -d build_temp temp_node
west flash -d build_gps gps_node
```

## System Architecture

### Node Communication
![Node Communication](node_communication.txt)

1. Sensor nodes collect data and transmit via CAN bus
2. VCU processes CAN messages
3. Data transmitted to cloud via MQTT
4. V2V communication via BLE

### Security Features
- Secure boot enabled
- Encrypted CAN messages
- TLS for MQTT communication
- BLE security level 2 for configuration

## API Documentation
[Link to detailed API documentation](api/README.md)

## Testing
Run the test suite:
```bash
cd tests
west build -b native_posix
west build -t run
```
