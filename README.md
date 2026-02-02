# Project Odradek

**A Death Stranding-inspired weather warning desk lamp with solenoid actuation, RGB lighting, and web-based control.**

![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)
![Status](https://img.shields.io/badge/status-active-success.svg)

---

## 📖 Overview

Project Odradek is a functional recreation of the Odradek terrainn scanner device from Death Stranding. It serves as an automated weather warning system that provides a 30-minute in advance alert before rain, complete with tons of cool visual LED effects, Buzzer queues to add a techy layer, and mechanical solenoid actuation for the flaps. The device also features a web based interface for full control and monitoring of the ESP-32's performance on both cores.

**Key Features:**
- 30-minute advance rain warning using real-time weather data
- Solenoid-actuated mechanical movement
- 255 individually addressable RGB LEDs
- Capacitive touch controls
- Web-based control interface with Death Stranding retro aesthetic
- Multi-location weather monitoring
- Custom audio chirp patterns (88.999% accurate to in-game sound)
- OTA firmware updates in web interface.
- Alarm system with gentle wake sequences

---

## 🎯 Features

### Hardware
- **Mechanical actuation** via 12V solenoid (60N force)
- **255 RGB LEDs** with breathing animations, ripple effects, and reactive patterns
- **Capacitive touch sensor** for mode switching and futureproofing
- **Piezo buzzer** with harmonically tuned Death Stranding audio
- **Dual-core ESP32** processing (Core 0: networking, Core 1: animations)

### Software
- **Real-time weather monitoring** from Open-Meteo API (I owe em so much for their great controbution)
- **30-minute rain prediction** with automatic alerts
- **Web interface** for remote control and monitoring
- **Touch gestures thingy:**
  - Double tap → Neutral white lamp mode
  - Triple tap → Warm yellow lamp mode
  - Quad tap → Warm white lamp mode
  - Long press → Standby mode / shutdown
- **System monitoring:** WiFi strength in dBm, CPU load on both cores, memory usage
- **Alarm system** with morning wake and reminder alerts
- **OTA updates** for easy firmware upgrades for futureproofing !!

### Animations to flex on your friends lol
- Territory Scan
- Strand Calibration
- Doom's Resonance
- Phantom Detection
- Chiral Density Test
- Voidout Simulation
- Location-specific weather pulses (customizable)

---

## 🛠️ Hardware Requirements

### Core Components
| Component | Specification | Quantity |
|-----------|--------------|----------|
| ESP32 Development Board | Any ESP32 with WiFi | 1 |
| Solenoid Actuator | 12V, 3A, 60N push-pull | 2 (1 active)* |
| Power Supply | 24V, 10A | 1 |
| Buck Converter | 24V → 12V, high current | 1 |
| Buck Converter | 24V → 5V for ESP32 | 1 |
| RGB LED Strip | WS2812B or similar, 255 LEDs | 1 |
| Capacitive Touch Sensor | TTP223 or similar | 1 |
| Piezo Buzzer | 3-8kHz range | 1 |

**Note:** Originally designed for 2 solenoids, but single-solenoid operation proved more efficient.

### Protection & Power Components
| Component | Specification | Quantity |
|-----------|--------------|----------|
| MOSFETs | Logic-level, suitable for 12V/3A | 2+ |
| Flyback Diodes | Fast recovery, rated for solenoid current | 2+ |
| Capacitors | 10µF + 0.1µF ceramic for buck converter stabilization | Multiple |
| Resistors | Various (10kΩ, 220Ω, etc.) | Multiple |
| Fuses | Various ratings for circuit protection | Multiple |
| 18 AWG Wire | For power distribution | As needed |

### Pinout (Default)
```
SOLENOID_PIN:   18
LED_DATA_PIN:   27
BUZZER_PIN:     26
TOUCH_PIN:      4
```

---

## 📦 Software Requirements

### Arduino Libraries
```cpp
#include <WiFi.h>
#include <Update.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPClient.h>      
#include <ArduinoJson.h>    
#include <esp_task_wdt.h>
#include "time.h"
```

Install via Arduino Library Manager:
- Adafruit NeoPixel
- ArduinoJson
- And perhaps whatever the IDE will cry about (On A Real Note: Ensure all dependencies are updated to the latest stable versions to avoid compilation conflicts.)

---

## 🚀 Installation

### 1. Hardware Assembly

**Power Distribution:**
```
24V PSU → Buck Converter 1 (→12V) → Solenoid
       → Buck Converter 2 (→5V) → ESP32
      → Buck Converter 3 (→5V) → Led Strip
```

**Safety Notes:**
- Use flyback diodes across solenoid terminals (observe polarity so you dont royally mess up like I did !!!)
- Add fuses to protect buck converters
- Stabilize 5V output with 10µF + 0.1µF capacitors so your ESP32 doesn's spaz out
- Double-check polarity before powering on
- Use MOSFETs with appropriate current ratings
- Never connect solenoid directly to ESP32 pins or you may slow fry the ESP32

**LED Strip:**
- Connect data line to GPIO 27
- Power from 12V rail (or 5V depending on strip)
- Add capacitor near strip power connection

### 2. Firmware Setup

**Configure WiFi:**
```cpp
const char* ssid = "[YOUR_WIFI_SSID]";
const char* password = "[YOUR_WIFI_PASSWORD]";
```

**Configure Weather Locations:**
Edit the weather API URLs with your coordinates:
```cpp
const char* weatherLocation1 = "https://api.open-meteo.com/v1/forecast?latitude=[YOUR_LAT]&longitude=[YOUR_LON]&current=rain,relative_humidity_2m,weather_code,temperature_2m&hourly=rain,precipitation_probability&timezone=auto";
```

Get your coordinates from [Open-Meteo](https://open-meteo.com/)

**Adjust LED Count:**
```cpp
#define NUM_LEDS 255  // Change to match your strip, i needed to shorten due to my small cat destroying my Strip while I was away.
```

### 3. Upload Firmware

1. Connect ESP32 via USB
2. Select board: **ESP32 Dev Module**
3. Upload `Odradek_OS_SYNOPTIC.ino`
4. Monitor serial output (115200 baud)

### 4. Access Web Interface

After successful boot:
1. ESP32 will connect to your WiFi
2. Check serial monitor for IP address
3. Open browser: `http://odradek.local`
4. Web interface features retro Death Stranding aesthetic with scanline effects, mainly based on Death Stranding 1's Bridges aesthetic 

---

## 🎮 Usage

### Touch Controls
| Gesture | Action |
|---------|--------|
| **Double Tap** | Toggle neutral white lamp mode |
| **Triple Tap** | Toggle warm yellow lamp mode |
| **Quad Tap** | Toggle warm white lamp mode |
| **Long Press (2s)** | Enter/exit standby mode |

### Web Interface
- **System Stats:** Real-time WiFi, CPU, memory monitoring using AJAX
- **Weather Dashboard:** Multi-location weather with rain predictions
- **Manual Controls:** Brightness, solenoid toggle, buzzer mute
- **Animations:** Trigger Death Stranding themed sequences
- **Alarms:** Set morning wake or reminder alerts
- **OTA Updates:** Upload new firmware wirelessly

### Automatic Weather Warnings
- Monitors weather every 2 minutes
- Triggers visual/audio alert 30 minutes before predicted rain
- Only activates between 10 AM - 9 PM (configurable, but aye I wanna sleep without jumpscares)

### Special Reactions
- **Cold Shiver:** Triggers once per day when temperature drops below 7°C
- **CPU Stress:** Visual alert if CPU load exceeds 85%
- **Low Memory:** Automatic restart if free heap drops below 10KB

---

## 🔧 Configuration

### Time Restrictions
Adjust reaction hours (prevent 3 AM alerts):
```cpp
const int REACTION_START_HOUR = 10;  // 10 AM
const int REACTION_END_HOUR = 21;    // 9 PM
```

### Weather Check Interval
```cpp
const int weatherCheckInterval = 120000;  // 2 minutes (in milliseconds)
```

### LED Brightness
```cpp
volatile int ledBrightness = 150;  // 0-255
```

---

## 🐛 Troubleshooting

### ESP32 Won't Boot
- Check 5V buck converter output (measure with multimeter)
- Verify capacitor polarity on power supply
- Ensure ESP32 isn't receiving more than 5V on VIN

### Solenoid Not Responding
- Verify MOSFET connections (Gate to ESP32, Source to GND, Drain to solenoid. Or if it is not completely fried like i had to figure ou the hard way)
- Check flyback diode orientation (cathode to +12V side)
- Test solenoid directly with 12V to confirm it works
- Measure voltage at solenoid during activation

### WiFi Connection Fails
- Check SSID/password in code
- Verify 2.4GHz network (ESP32 doesn't support 5GHz due to hardware)
- Monitor serial output for error messages
- Try moving closer to router

### LEDs Not Lighting
- Verify WS2812B strip power (5V or 12V depending on model, for me 5V)
- Check data line connection to GPIO 27
- Confirm `NUM_LEDS` matches your strip length
- Test with simple Adafruit NeoPixel example first

### Web Interface Not Loading
- Confirm ESP32 obtained IP address (check serial monitor)
- Verify you're on same WiFi network
- Try accessing via mDNS: `http://odradek.local` (if configured)
- Check browser console for JavaScript errors

---

## 📚 Parts List & Links

A detailed parts list with Amazon/supplier links can be found in the `PARTS.md` file (coming soon).

**Estimated Total Cost:** ~€230-300 (depending on suppliers and quantities)

---

## 🛡️ Safety Warning

**This project involves:**
- High current (24V, 10A) power supplies, so don't get embers flying on your hands like I did
- Solenoid actuation with significant mechanical force near fragile objects
- Potential for component damage if wired incorrectly
- Potential combustion of diodes due to incorrect polarity or accidental MOSFET shorting.

**Always:**
- Double-check polarity before powering on
- Use appropriate fuses and protection circuitry
- NEVER bypass safety components (diodes, fuses)
- Test with multimeter before connecting ESP32
- Work in a well-ventilated area
- Have fire safety equipment nearby when testing

**The author is not responsible for damage to components, property, or injury. Build at your own risk.**

---

## 🚧 Future Improvements

- [ ] 3D printed enclosure with arm mechanism
- [ ] Second solenoid for dual-arm actuation if I were to go wild
- [ ] MQTT integration for Home Assistant to futureproof
- [ ] Mobile app control
- [ ] Multi-timezone support
- [ ] Custom alarm sounds/patterns

---

## 📝 License

This project is licensed under the **MIT License** - see below for details.

```
MIT License

Copyright (c) 2026 Madis O.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 🙏 Credits & Inspiration

**Inspired by:**
- *Death Stranding* & *Death Stranding 2* by Kojima Productions
- The Odradek scanner device design

**Special Thanks to:**
- Hideo Kojima for creating the Death Stranding universe (THE GOAT)
- Open-Meteo for free weather API 
- Adafruit for excellent hardware libraries
- The ESP32 community for extensive documentation
- Claude AI & xAI Grok supporting me in Debugging process 

**Built by:** Madis O. ([@nullopses](https://instagram.com/nullopses)) ([@Madis_.o](https://www.instagram.com/madis_.o))

**Part of:** NullSec Projects

---

## 📖 Additional Documentation

- **[BUILD_LOG.md](BUILD_LOG.md)** - Full development journey with failures, learning, and burnt components
- **PARTS.md** *(coming soon)* - Detailed parts list with supplier links
- **WIRING.md** *(coming soon)* - Complete wiring diagrams and schematics

---

**⚠️ Note:** This is a hobby project built by a student. Code may contain inefficiencies or unconventional approaches. Suggestions and improvements welcome via issues/PRs.

**If you build one, tag [@nullopses](https://instagram.com/nullopses) - I'd love to see it !!**
