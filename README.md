# Smart Vehicle Tracking and Diagnostics System

## Overview
A production-ready vehicle tracking and diagnostics system using Zephyr RTOS on ESP32-S3. Features essential vehicle telemetry, diagnostics, and ASIL-B safety features.

## Core Features
- Real-time sensor monitoring (temperature, battery, brakes, tire pressure)
- CAN-based inter-ECU communication with ISO-TP support
- MQTT telemetry for remote monitoring
- BLE configuration interface
- ASIL-B compliant safety monitoring

## Experimental Features
*The following features are under development and not part of the core system:*
- V2X Communication
  - V2V (Vehicle-to-Vehicle): Experimental BLE-based safety alerts
  - V2I (Vehicle-to-Infrastructure): Basic MQTT infrastructure status
  - V2N (Vehicle-to-Network): Cloud-based predictive maintenance

## Safety & Diagnostics
- Task & runtime monitoring
- Memory protection
- Error detection & recovery
- UDS diagnostic services
- Real-time statistics
