# Saloon Doors

## Overview

Saloon Doors is an ESP8266-based motion-triggered firing control system designed for interactive installations. The project uses accelerometer/gyroscope sensors (MPU6050) to detect motion and trigger firing mechanisms with customizable timing parameters. The system also provides a web-based interface for remote control, monitoring, and configuration.

## Hardware Requirements

- ESP8266 (ESP12E) microcontroller
- Two MPU6050 accelerometer/gyroscope sensors (I2C addresses 0x68 and 0x69)
- Two fire control pins (D5/GPIO14 and D6/GPIO12) for controlling firing mechanisms
- Two manual trigger buttons (D3/GPIO0 and D4/GPIO2)
- Power supply appropriate for your firing mechanisms

## Features

- **Motion-Based Triggering**: Automatically activates firing mechanisms based on gyroscope readings
- **Manual Triggering**: Support for physical buttons and remote web-based triggering
- **Adjustable Parameters**:
  - Min/Max fire time (1.0-6.0 seconds default)
  - Remote fire time (2.5 seconds default)
  - Reset limit (3.0 seconds default)
  - Fire cycle timing (0.5 seconds default)
  - Min/Max gyro sensitivity thresholds (50-750 degrees/second default)
- **Web Interface**: Access control panel from any device on the same network
- **Statistics Tracking**: Monitors system performance and usage metrics including:
  - Remote trigger count
  - Acceleration trigger count
  - Highest acceleration reading
  - Total fire time
  - Longest fire duration
  - Average acceleration trigger value
  - Peak memory usage
  - Highest gyro reading
- **Configuration Persistence**: Settings saved to EEPROM and retained between power cycles
- **Watchdog Timer**: System automatically recovers from crashes or hangs

## Web Interface Pages

The system hosts a web server with the following pages:

1. **Root Page**: Main navigation menu
2. **Data Page**: Real-time sensor data display
3. **Fire Control**: Manual triggering and status monitoring
4. **Fire Settings**: Configuration of firing parameters
5. **Statistics**: System performance and usage metrics

## Pin Configuration

| Function         | Pin          | Description                      |
|------------------|--------------|----------------------------------|
| FIRE_PIN_1       | D5 (GPIO14)  | Controls first firing mechanism  |
| FIRE_PIN_2       | D6 (GPIO12)  | Controls second firing mechanism |
| MANUAL_TRIGGER_1 | D3 (GPIO0)   | Physical button for manual firing|
| MANUAL_TRIGGER_2 | D4 (GPIO2)   | Physical button for manual firing|

## Fire Control Logic

- The system alternates between the two firing pins based on the configured fire cycle
- When motion exceeds the minimum gyro threshold, firing duration is calculated proportionally to motion intensity
- A reset period prevents rapid re-triggering
- Remote triggering uses a fixed fire duration

## Installation and Setup

1. Clone this repository
2. Open the project in PlatformIO
3. Configure WiFi settings in `src/main.cpp` if needed
4. Upload to your ESP8266
5. Connect to the device's WiFi access point or configure it to join your network
6. Access the web interface by navigating to the device's IP address

## Accessing the Web Interface

### First-time Setup

When the system first boots or can't connect to a configured WiFi network, it creates its own access point:

1. On your phone or laptop, go to WiFi settings and look for a network named "HighNoon"
2. Connect to this network using the password "shaboinky"
3. Once connected, a captive portal may automatically open or you can manually navigate to http://192.168.4.1
4. From here, you can configure the device to connect to your home/venue WiFi network

### Accessing on a Local Network

Once the system is configured to join your local WiFi:

1. Find the device's IP address using one of these methods:
   - Check your router's connected devices list
   - Use the serial monitor during boot-up (connect via USB)
   - If using DHCP, the device will typically have the same IP each time

2. **From a Laptop/Desktop**:
   - Open any web browser (Chrome, Firefox, Safari, etc.)
   - Enter the device's IP address in the address bar (e.g., http://192.168.1.100)
   - Bookmark this page for easy access in the future

3. **From a Smartphone**:
   - Open your mobile browser (Chrome, Safari, Firefox)
   - Enter the device's IP address in the address bar
   - For easier access:
     - On iOS: Add to Home Screen from the share menu
     - On Android: Add to Home Screen or Bookmark from the menu
   - The interface is mobile-responsive and works on all modern mobile browsers

### Troubleshooting Connection Issues

- If you can't access the web interface, ensure your device is on the same WiFi network
- If using a VPN on your phone/laptop, temporarily disable it
- Try accessing by IP address (e.g., http://192.168.1.100) rather than hostname
- If the system creates its own access point repeatedly, check your WiFi credentials or try resetting the device

## Dependencies

- ESP8266WiFi
- ESP8266WebServer
- Wire (I2C communication)
- EEPROM (settings storage)
- DNSServer
- MPU6050 (pkourany/MPU6050@^1.0.3)
- ESP_WifiManager (khoih-prog/ESP_WifiManager@^1.12.1)
- ArduinoJson (bblanchon/ArduinoJson@^6.20.0)

## Configuration

All settings can be adjusted through the web interface. Default settings are restored by navigating to the Fire Settings page and clicking "Reset to Defaults".

## Troubleshooting

- **WiFi Connection Issues**: The system creates its own access point if it can't connect to a configured network
- **Sensor Not Responding**: Check I2C wiring and addresses
- **Erratic Behavior**: Reset the system using the built-in reset button or power cycle
- **Fire Mechanism Not Activating**: Verify pin connections and ensure adequate power supply

## Development

The project is structured as follows:

- `src/main.cpp`: Main application code containing all functionality
- `platformio.ini`: PlatformIO configuration and dependencies
- `lib/`: Contains any custom libraries (currently empty)

To extend functionality, modify the main.cpp file or create additional libraries in the lib directory.

## Safety Considerations

This system was designed to control electronic firing mechanisms. Always ensure:

1. All firing mechanisms have appropriate safety measures
2. The system is properly isolated from flammable materials
3. Manual emergency shutoff is available
4. Local regulations regarding electronic control systems are followed

## Future Improvements

- Separate code into multiple files for better organization
- Add authentication to the web interface
- Support for more types of sensors
- Extended logging and diagnostics
- Remote firmware updates

## License

This project is licensed under the MIT License - see the LICENSE file for details. 