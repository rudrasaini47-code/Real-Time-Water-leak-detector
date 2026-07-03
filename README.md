# Real Time Water Leak Detector
 
> **Smart water pipeline leak detection system with automatic valve shutoff, real-time web dashboard, and WiFi monitoring — built with Arduino Uno, NodeMCU ESP8266, and YF-S201 flow sensors.**
 
---
 
##  Demo
 
```
Flow IN: 14.20 L/min  |  Flow OUT: 11.60 L/min  |  Diff: 2.60 L/min  →   LEAK DETECTED → True
```
 
Live dashboard updates every 2 seconds. Leak triggers browser notification + sound alert + automatic valve shutoff within 3 seconds of detection.
 
---
 
##  Features
 
- **Real-time flow monitoring** — measures Flow IN and Flow OUT simultaneously using YF-S201 Hall-effect sensors
- **Intelligent leak detection** — compares flow difference mathematically (no moisture sensor needed)
- **False alarm prevention** — requires 3 consecutive seconds of sustained difference before triggering
- **Automatic valve shutoff** — 12V solenoid valve closes instantly when leak is confirmed
- **Manual reset** — physical button to reopen valve after fixing the leak
- **WiFi dashboard** — live web UI accessible from any browser on the same network
- **Multi-alert system** — in-app toast, browser push notification, and sound alert on leak
- **No cloud needed** — ESP8266 hosts its own API endpoint on local WiFi
- **Works with hotspot** — connect ESP8266 to mobile hotspot for portable use
---
 
##  Hardware Required
 
| Component | Quantity | Notes |
|---|---|---|
| Arduino Uno (R3) | 1 | Any revision |
| NodeMCU v1.0 (ESP8266) | 1 | Has onboard USB — easiest to flash |
| YF-S201 Flow Sensor | 2 | One inlet, one outlet |
| 12V Normally Closed Solenoid Valve | 1 | 1/2 inch — search "12V NC solenoid valve" |
| 12V DC Power Adapter | 1 | 1A rating sufficient |
| 1kΩ Resistor | 1 | Voltage divider |
| 2kΩ Resistor | 1 | Voltage divider (or 2× 1kΩ in series) |
| Momentary Push Button | 1 | Manual valve reset |
| Breadboard + Jumper Wires | — | Male-to-male and male-to-female |

 
##  Wiring
 
### Arduino Uno Pin Connections
 

| `2` (Digital) | YF-S201 Flow IN signal (yellow wire) |
| `3` (Digital) | YF-S201 Flow OUT signal (yellow wire) |
| `5` (Digital) | Relay module IN |
| `6` (Digital) | Reset button (other leg to GND) |
| `TX` (pin 1) | NodeMCU RX via voltage divider |
| `RX` (pin 0) | NodeMCU TX (direct) |
| `5V` | Both YF-S201 VCC (red wires) + Relay VCC |
| `3.3V` | NodeMCU 3V3 pin |
| `GND` | All component GNDs (shared) |
 
### NodeMCU v1.0 Pin Connections
 
| NodeMCU Pin | Connected To |
|---|---|
| `3V3` | Arduino 3.3V |
| `GND` | Arduino GND (shared) |
| `D9 / RX` | Arduino TX (pin 1) via voltage divider |
| `D10 / TX` | Arduino RX (pin 0) direct |
 
> All other NodeMCU pins are left unconnected.
 
### Voltage Divider (mandatory — protects NodeMCU RX from 5V)
 
```
Arduino TX (pin 1) ──[1kΩ]──┬──── NodeMCU RX (D9)
                             │
                           [2kΩ]
                             │
                            GND
 
Output voltage: 5V × (2kΩ / 3kΩ) = 3.33V ✓
```
 
##  File Structure
 
```
FlowWatch
│
├── arduino_flowwatch.ino     ← Upload to Arduino Uno
├── esp8266_flowwatch.ino     ← Upload to NodeMCU ESP8266
├── flow-monitor.html         ← Open in browser for dashboard
└── README.md
```
 
---
 
##  Setup
 
### Step 1 — Configure WiFi credentials
 
Open `esp8266_flowwatch.ino` and set your WiFi name and password:
 
```cpp
const char* WIFI_SSID     = "Your_WiFi_Or_Hotspot_Name";
const char* WIFI_PASSWORD = "yourpassword";
```
 
> **Note:** ESP8266 only supports 2.4GHz WiFi. If using Android hotspot, force it to 2.4GHz in hotspot settings.
 
### Step 2 — Apply NC valve code changes
 
Since the solenoid valve is Normally Closed, update these 3 lines in `arduino_flowwatch_v2.ino`:
 
```cpp
// In setup() — change LOW to HIGH (NC valve needs power to stay open)
digitalWrite(VALVE_PIN, HIGH);
 
// closeValve() function
void closeValve() { digitalWrite(VALVE_PIN, LOW);  valveOpen = false; }
 
// openValve() function
void openValve()  { digitalWrite(VALVE_PIN, HIGH); valveOpen = true;  }
```
 
### Step 3 — Upload Arduino code
 
1. **Disconnect NodeMCU TX/RX wires** from Arduino pins 0 and 1
2. Connect Arduino to laptop via USB
3. Arduino IDE → Tools → Board → **Arduino Uno**
4. Upload `arduino_flowwatch.ino`
5. Open Serial Monitor at **9600 baud** — verify output looks like:
```
   IN:0.00,OUT:0.00,LEAK:0,VALVE:1,DIFF:0.00
```
6. Close Serial Monitor → reconnect NodeMCU TX/RX wires
### Step 4 — Upload ESP8266 code
 
1. Connect **NodeMCU** to laptop via USB (Arduino disconnected)
2. Arduino IDE → Tools → Board → **NodeMCU 1.0 (ESP-12E Module)**
3. Tools → Upload Speed → **115200**
4. Upload `esp8266_flowwatch.ino`
5. LED blinks while connecting to WiFi, goes solid when connected
### Step 5 — Find NodeMCU IP address
 
- **Mobile hotspot:** Check connected devices list in your hotspot settings — look for "espressif"
- **Home router:** Open router admin page → connected devices → find "ESP8266"
- **Verify:** Open `http://<ESP_IP>/` in browser — status page should load with live readings
### Step 6 — Open dashboard
 
1. Open `flow-monitor.html` in Chrome or Edge
2. Enter API URL: `http://<ESP_IP>/api/flow-data`
3. Click **▶ Start Polling**
4. Allow browser notification permission
5. All 5 cards update in real time 
---
 
##  How Leak Detection Works
 
```
Every 1 second:
  flowDiff = flowIn − flowOut
 
  if flowDiff < 2.0  →  Normal, reset counter
  if flowDiff ≥ 2.0  →  Increment confirmation counter
  if counter ≥ 3     →  LEAK CONFIRMED
                           → valve closes
                           → dashboard alerts fire
                           → browser notification sent
                           → sound alert plays
 
Press reset button (D6) after fixing leak → valve reopens
```
 
The 3-second confirmation window prevents false alarms from sensor jitter when taps open/close suddenly.
 
---
 
##  API Reference
 
The NodeMCU hosts a JSON REST API on port 80.
 
### `GET /api/flow-data`
 
```json
{
  "flow_in": 14.20,
  "flow_out": 11.60,
  "flow_diff": 2.60,
  "leak": 1,
  "valve_open": 0,
  "uptime_s": 3420,
  "has_data": true,
  "stale": false
}
```
 
| Field | Type | Description |
|---|---|---|
| `flow_in` | float | Inlet flow rate in L/min |
| `flow_out` | float | Outlet flow rate in L/min |
| `flow_diff` | float | Difference (flow_in − flow_out) |
| `leak` | int | 0 = normal, 1 = leak detected |
| `valve_open` | int | 1 = valve open, 0 = valve closed |
| `uptime_s` | int | ESP8266 uptime in seconds |
| `has_data` | bool | False until first Arduino reading received |
| `stale` | bool | True if no update received in 10+ seconds |
 
### `GET /`
 
Human-readable status page. Auto-refreshes every 2 seconds.
 
---
 
##  Configuration
 
Adjustable constants in `arduino_flowwatch.ino`:
 
```cpp
const float LEAK_THRESHOLD    = 2.0;   // L/min difference to trigger alert
const int   CONFIRM_COUNT     = 3;     // Consecutive detections before confirming
const float CALIBRATION_FACTOR = 7.5;  // YF-S201: pulses/sec per L/min
const unsigned long SEND_INTERVAL = 2000;  // How often to send data to ESP (ms)
```
 
**Tuning tips:**
- Too many false alarms → increase `LEAK_THRESHOLD` to 3.0 or `CONFIRM_COUNT` to 5
- Missing real leaks → decrease `LEAK_THRESHOLD` to 1.0 or 1.5
- Readings seem off → adjust `CALIBRATION_FACTOR` (valid range: 4.5 – 8.5 for YF-S201)
---
 
##  Important Notes
 
### Upload rule
**Always disconnect NodeMCU TX/RX wires before uploading to Arduino.** Arduino Uno shares its only serial port (pins 0/1) with USB. Reconnect after upload.
 
### Power
- Arduino powered by USB (any 5V phone charger or laptop)
- NodeMCU powered from Arduino 3.3V pin — **do not connect NodeMCU USB and Arduino 3.3V simultaneously**
- Solenoid valve powered by separate 12V adapter
### Flyback diode
Always install the 1N4007 diode directly across solenoid valve terminals. Without it, voltage spikes from the solenoid coil can damage the relay and Arduino over time.
 
### Network
The device running the dashboard must be on the **same WiFi/hotspot** as the NodeMCU. The NodeMCU IP may change after power cycles — set a static DHCP lease in your router to fix it permanently.
 
---
 
##  Troubleshooting
 
| Problem | Cause | Fix |
|---|---|---|
| Dashboard shows all zeros | Serial Monitor open | Close Serial Monitor — it conflicts with ESP communication |
| Dashboard shows all zeros | Two power sources on NodeMCU | Remove USB from NodeMCU — power only from Arduino 3.3V |
| Flow always reads 0.00 | Wrong interrupt pins | YF-S201 signal must go to pin **2** and pin **3** on Uno |
| Valve stays closed on boot | NC code changes not applied | Apply the 3 changes in Step 2 of setup |
| ESP not connecting to WiFi | Wrong credentials or 5GHz | Double-check SSID/password. Force Android hotspot to 2.4GHz |
| ESP broadcasting own WiFi | Failed to join network | Same as above — ESP falls back to AP mode on failure |
| Upload fails | NodeMCU TX/RX still connected | Disconnect pins 0 and 1 from NodeMCU before uploading |
| False leak alerts | Sensor jitter | Increase `LEAK_THRESHOLD` to 3.0 or `CONFIRM_COUNT` to 5 |
| Random ESP resets | Power instability | Add 100µF capacitor between NodeMCU 3V3 and GND |
 
---
 
 
## Built With
 
- **Arduino Uno** — sensor reading, flow calculation, valve control
- **NodeMCU ESP8266** — WiFi connectivity, JSON API server
- **YF-S201** — Hall-effect pulse flow sensors
- **Arduino IDE** — firmware development
- **Chart.js** — real-time flow history chart in dashboard
- **Vanilla HTML/CSS/JS** — zero-dependency web dashboard
---
 
