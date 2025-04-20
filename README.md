# Smart Vehicle Tracking and Diagnostics System

## Overview

This project implements a Smart Vehicle Tracking and Diagnostics System using Zephyr RTOS on ESP32-S3. It integrates real-time vehicle diagnostics, telemetry data monitoring, and remote accessibility. The system leverages a variety of sensors, distributed ECUs, and robust communication protocols to provide comprehensive insights into vehicle health and performance.

**Experimental V2X Update:**  
*This section describes the experimental Vehicle-to-Everything (V2X) features currently under development. These features enable advanced communication beyond the vehicle’s internal network, facilitating direct communication with other vehicles, infrastructure, and cloud services.*

- **V2N (Vehicle-to-Network):** Uses MQTT over Wi‑Fi for remote data transmission. In addition to traditional telemetry, it now supports traffic updates, hazard notifications, and cloud-based predictive maintenance.
- **V2V (Vehicle-to-Vehicle):** Implements short-range communication (via BLE) to exchange real-time safety and collision avoidance data directly with nearby vehicles.
- **V2I (Vehicle-to-Infrastructure):** Leverages MQTT communication with edge devices (e.g., traffic light controllers, Raspberry Pi units) to relay infrastructure status and receive real-time updates for smart traffic management.

## Key Components

### Sensors

- **Battery Level Sensor:** Monitors vehicle battery status.
- **Tire Pressure Sensor (TPMS):** Checks tire pressure levels.
- **Collision Sensors:** Detects collisions or nearby obstacles.
- **GPS Module:** Tracks the real-time location of the vehicle.
- **Engine Temperature Sensor:** Monitors engine temperature.
- **Speed Sensor:** Tracks vehicle speed to ensure compliance with speed limits.
- **Brake Monitoring:** Tracks brake status and functionality.

### ECU (Electronic Control Unit)

Primary controller based on ESP32-S3 for data collection, processing, and communication. Interfaces with various sensors using CAN (with ISO-TP, J1939, and CAN FD), MQTT, BLE, and Wi‑Fi protocols. Implements secure boot, encrypted storage, and ASIL-B safety features.

## Communication Protocols

- **CAN Bus:** Facilitates in-vehicle communication between controllers and the ECU. Supports ISO-TP, J1939, and CAN FD.
- **MQTT:** Enables remote data transmission to cloud servers and mobile applications. Now extended to support V2X:
  - **V2N (Cloud Connectivity):** Transmits advanced telemetry such as traffic updates and predictive maintenance alerts.
  - **V2I (Edge Connectivity):** Communicates with local infrastructure for smart traffic management.
- **BLE:** Provides mobile app connectivity for easy configuration and V2V communication.
- **Wi‑Fi:** Supports remote monitoring and cloud connectivity.
- **ESPNOW / Wi‑Fi Direct (Experimental V2V):** Implements short-range, low-latency communications between nearby vehicles for safety and collision avoidance.

## Software Platform

- **Zephyr RTOS:** Manages real-time tasks, prioritizing critical functions such as engine temperature monitoring and speed violation alerts. Provides hardware abstraction, task scheduling, and safety/security features.
- **ASIL-B Safety:** Implements task monitoring, memory protection, redundancy, error recovery, and diagnostics.
- **Diagnostics:** UDS services, error memory, sensor status, and self-test routines.
- **Signal/Event System:** AUTOSAR-inspired, for inter-process and inter-node communication.

## System Architecture

- Distributed ECUs for each sensor domain (temperature, GPS, collision, battery, brake, TPMS, speed).
- Central VCU for CAN routing, V2X, diagnostics, and cloud communication.
- Modular, extensible, and production-grade codebase.

---
