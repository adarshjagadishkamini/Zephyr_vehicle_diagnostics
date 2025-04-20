# Diagnostic Services API

## UDS Services
- **ReadDataByIdentifier (0x22)**
  - DID 0xF190: Vehicle Information
  - DID 0xF183: ECU Software Version
  - DID 0xF120: Sensor Status
  - DID 0xF150: Error Memory (DTCs)

## Security Access (0x27)
Security levels:
- Level 1: Basic diagnostics
- Level 2: Calibration
- Level 3: Programming

## Routine Control (0x31)
Supported routines:
- Self Test (0x0100)
- Sensor Calibration (0x0200)
- Memory Check (0x0300)

## Error Memory
- Standard OBD-II DTCs
- Supplementary system-specific DTCs
- Extended data records for each DTC
