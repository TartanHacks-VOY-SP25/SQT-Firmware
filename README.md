# SQT - Shipping Quality Tracker

![Project Image](SQT.png)  
*Replace the above with an actual image path once available.*

## Overview
SQT is a hardware device developed for the **Tartanhacks Spring 2025 Hackathon**. It is designed to be placed inside shipping containers to monitor and assess the handling practices of cargo companies. By tracking environmental and motion data, SQT helps ensure that shipments are handled with care.

## Features
- **OLED Display**: Provides real-time status updates.
- **IMU (Inertial Measurement Unit)**: Detects movement and potential mishandling.
- **Temperature & Humidity Sensor**: Monitors environmental conditions.
- **Battery Powered**: Designed for portability and long-duration operation.
- **Built with ESP-IDF & FreeRTOS**: Ensures efficient multitasking and robust performance.

## Technologies Used
- **ESP-IDF** (Espressif IoT Development Framework)
- **FreeRTOS** (Real-Time Operating System)
- **Embedded C**
- **Hardware Sensors & OLED Display**

## Installation & Setup
1. **Clone the Repository**:
   ```sh
   git clone https://github.com/your-repo/sqt.git
   cd sqt
   ```
2. **Set up ESP-IDF**:
   - Follow the official [ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).
   - Export the required environment variables.
3. **Build & Flash the Firmware**:
   ```sh
   idf.py set-target esp32
   idf.py build
   idf.py flash
   ```
4. **Monitor Logs**:
   ```sh
   idf.py monitor
   ```

## Usage
- Place the SQT device inside a shipping container.
- Data is collected and displayed on the OLED screen.
- Retrieve logs via serial output for further analysis.

## Future Enhancements
- Cloud integration for remote monitoring
- AI-based anomaly detection for mishandling events

