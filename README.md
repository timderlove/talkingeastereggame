# talkingeastereggame


# quick about the project #

building talking easter eggs with esp32-c3 supermini for a fun game for my son who was born blind. ive been working on this for the past 5 years and have made improvements as the years have gone on. this will be my first time on github and id like to see what happens i want to know if gemini did this correctly. i have very simple introduction to some code and look at what the ai wrote and scratch my head a lot. basically if there was a better way to code it or how i can improve over the years meaning next year and the year after let me know thank you!!!!
also im still proofreading this whole thing ai wrote for me to not sound completely simple 


# Smart Easter Egg (Hide & Seek Edition)

An interactive, talking Smart Easter Egg powered by an ESP32-C3 microcontroller. The device uses an ultrasonic distance sensor to detect when a player approaches or picks up the egg, triggering interactive sound effects, custom voice lines, and proximity-based audio themes (such as the Super Mario Bros. melody and interactive game states).

## 🚀 Features
- **Proximity-Based Audio Engine:** Mathematically synthesized tones (Mario Theme, Coin Synth) and localized `.wav` playback direct from flash memory.
- **Hide & Seek Game Mechanics:** Uses a dedicated toggle button to trigger game states with proximity thresholds ("Warm" vs "Hot").
- **Anti-Rattle & Pop Engineering:** Utilizes a non-blocking audio loop with automated I2S DMA buffer clearing and phase-resetting to eliminate speaker popping and clipping.
- **Hardware Stability Optimization:** Incorporates software-gated "sensor muting" and custom smoothing capacitors to prevent power sags, electrical rail noise, and hardware brownouts.

---

## 🛠️ Hardware Requirements

| Component | Purpose | Specification / Notes |
| :--- | :--- | :--- |
| **ESP32-C3 SuperMini** | Project Brain / MCU | Compact RISC-V development board with onboard BLE/Wi-Fi. |
| **MAX98357A** | I2S Audio Amplifier | Direct digital-to-analog audio amplification. |
| **HC-SR04** | Ultrasonic Distance Sensor | Proximity sensing ("eyes" of the egg). Requires stable 5V. |
| **HW-107 (TP4056)** | Battery Charger & Protection | 3.7V Lithium Polymer (LiPo) charging board. |
| **Push Button** | Game State Controller | Standard momentary tactile button to wake/start the game. |
| **Electrolytic Capacitor** | Inrush Protection | 100µF to 220µF (Rated $\ge$ 10V) to prevent over-current tripping. |
| **8Ω Speaker** | Audio Output | Connected directly to the MAX98357A terminal block. |

---

## 📌 Pin Configuration Matrix

Ensure your breadboard or custom Carrier PCB matches these exact GPIO definitions to prevent hardware or clock conflicts:


******ESP32-C3 SuperMini Pinout********
                     ._____________.
               5V  --| 1        16 |-- TX / GPIO21
              GND  --| 2        15 |-- RX / GPIO20
              3V3  --| 3        14 |-- GPIO10 (BUTTON_PIN)
  (I2S_BCLK) GPIO3 --| 4        13 |-- GPIO9
  (I2S_LRCK) GPIO4 --| 5        12 |-- GPIO8  (Onboard Blue LED)
  (I2S_DATA) GPIO5 --| 6        11 |-- GPIO7
  (TRIG_PIN) GPIO2 --| 7        10 |-- GPIO6  (ECHO_PIN)
             GPIO1 --| 8         9 |-- GPIO5
                     +-------------+

####### 📋 Wiring Schematics ######

1. **MAX98357A I2S Amplifier Connection:**
   - **LRC** ### 📋 Wiring Schematics

1. **MAX98357A I2S Amplifier Connection:**
   - **LRC** $\rightarrow$ **GPIO 4**
   - **BCLK** $\rightarrow$ **GPIO 3**
   - **DIN** $\rightarrow$ **GPIO 5**
   - **GAIN** $\rightarrow$ *Leave Floating* (Defaults to 9dB to prevent distortion/clipping).
   - **SD** $\rightarrow$ **3.3V** (Crucial: Must be pulled high to unmute the amplifier chip).
   - **GND** $\rightarrow$ **System GND**
   - **VIN** $\rightarrow$ **5V Rail**

2. **HC-SR04 Ultrasonic Distance Sensor Connection:**
   - **VCC** $\rightarrow$ **5V Rail** (Do not use 3.3V; low voltage causes sensor timeout lockups at `999`).
   - **Trig** $\rightarrow$ **GPIO 2**
   - **Echo** $\rightarrow$ **GPIO 6** (Moved to resolve input conflicts with I2S clocks).
   - **GND** $\rightarrow$ **System GND**

3. **Game State Button Connection:**
   - **Pin 1** $\rightarrow$ **GPIO 10**
   - **Pin 2** $\rightarrow$ **System GND**

---

## ⚡ Power Sub-System & Protection Circuitry

When the audio amplifier fires up, it pulls sudden pulses of power from the rails. This causes an inrush current spike that tricks the HW-107 (TP4056) protection chip into entering a locked over-current safety state, cutting off all device power.

### The Fix (Decoupling Capacitor Placement)
- Place a **100µF to 220µF electrolytic capacitor** directly parallel across the power rail load.
- **Positive Lead (+):** Connect directly to the **OUT+** pin of the HW-107 module (routing to the 5V/VCC pins of the ESP32 and MAX98357A).
- **Negative Lead (-):** Connect directly to the **OUT-** pin of the HW-107 module (Common System Ground).
- **Placement Proximity:** Keep the capacitor leads physically as close to the HW-107 output pads as possible to suppress voltage sags instantly before the chip registers a fault.

---

## 💻 Software & Firmware Setup

### 1. IDE Requirements
- Install the **Arduino IDE** (v2.0 or higher recommended).
- Add the ESP32 Board Manager URL under Preferences: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Install the latest **esp32** board package by Espressif.

### 2. Required Board Configurations
When preparing to flash your firmware, use these exact settings in the `Tools` menu:
- **Board:** `ESP32C3 Dev Module` (or your specific C3 variant)
- **USB CDC On Boot:** `Enabled` *(Crucial for serial logging over native USB)*
- **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)` *(Required to fit raw PROGMEM audio arrays inside flash memory)*

### 3. Audio File Requirements (`bunny_audio.h`)
If converting custom voices using an HTML PROGMEM hex converter, make sure your raw source file meets these constraints to keep audio clean and glitch-free:
- **Format:** `.wav` (16-bit PCM)
- **Sample Rate:** `16000 Hz` (or `24000 Hz` depending on firmware profile setup)
- **Channels:** `Mono` (Single Channel)

---

## 🛠️ Calibration & Diagnostics

### Troubleshooting Sensor "999" Timeouts
If the Serial Monitor is locked reading `999.00cm`, it means the Echo pulse isn't returning. Check the following:
1. **Voltage Sag:** Ensure the HC-SR04 is getting a true 5V. Laptop USB ports running on power-saver mode can drop below 4.7V, blinding the sensor. Use a strong powered hub or battery output.
2. **Software Shield Gating:** The firmware includes a built-in `is_audio_playing` safety gate. While the speaker is active, the ultrasonic sensor is temporarily muted to avoid false readings caused by acoustic vibrations and pin noise.
3. **The Swap Test:** If you receive a flat `999` loop, swap your physical **Trig** and **Echo** jumper positions on the breadboard.
   - **LRC**   >>>>>  **GPIO 4**
   - **BCLK**  >>>>>  **GPIO 3**
   - **DIN**   >>>>>  **GPIO 5**
   - **GAIN**  >>>>>  *Leave Floating* (Defaults to 9dB to prevent distortion/clipping).
   - **SD**    >>>>>  **3.3V** (Crucial: Must be pulled high to unmute the amplifier chip).
   - **GND**   >>>>>  **System GND**
   - **VIN**   >>>>>  **5V Rail**

2. **HC-SR04 Ultrasonic Distance Sensor Connection:**
   - **VCC**   >>>>>  **5V Rail** (Do not use 3.3V; low voltage causes sensor timeout lockups at `999`).
   - **Trig**  >>>>>  **GPIO 2**
   - **Echo**  >>>>>  **GPIO 6** (Moved to resolve input conflicts with I2S clocks).
   - **GND**   >>>>>  **System GND**

3. **Game State Button Connection:**
   - **Pin 1** >>>>>  **GPIO 10**
   - **Pin 2** >>>>>  **System GND**

---

## ⚡ Power Sub-System & Protection Circuitry

When the audio amplifier fires up, it pulls sudden pulses of power from the rails. This causes an inrush current spike that tricks the HW-107 (TP4056) protection chip into entering a locked over-current safety state, cutting off all device power.

### The Fix (Decoupling Capacitor Placement)
- Place a **100µF to 220µF electrolytic capacitor** directly parallel across the power rail load.
- **Positive Lead (+):** Connect directly to the **OUT+** pin of the HW-107 module (routing to the 5V/VCC pins of the ESP32 and MAX98357A).
- **Negative Lead (-):** Connect directly to the **OUT-** pin of the HW-107 module (Common System Ground).
- **Placement Proximity:** Keep the capacitor leads physically as close to the HW-107 output pads as possible to suppress voltage sags instantly before the chip registers a fault.

---

## 💻 Software & Firmware Setup

### 1. IDE Requirements
- Install the **Arduino IDE** (v2.0 or higher recommended).
- Add the ESP32 Board Manager URL under Preferences: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- Install the latest **esp32** board package by Espressif.

### 2. Required Board Configurations
When preparing to flash your firmware, use these exact settings in the `Tools` menu:
- **Board:** `ESP32C3 Dev Module` (or your specific C3 variant)
- **USB CDC On Boot:** `Enabled` *(Crucial for serial logging over native USB)*
- **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)` *(Required to fit raw PROGMEM audio arrays inside flash memory)*

### 3. Audio File Requirements (`bunny_audio.h`)
If converting custom voices using an HTML PROGMEM hex converter, make sure your raw source file meets these constraints to keep audio clean and glitch-free:
- **Format:** `.wav` (16-bit PCM)
- **Sample Rate:** `16000 Hz` (or `24000 Hz` depending on firmware profile setup)
- **Channels:** `Mono` (Single Channel)

---

## 🛠️ Calibration & Diagnostics

### Troubleshooting Sensor "999" Timeouts
If the Serial Monitor is locked reading `999.00cm`, it means the Echo pulse isn't returning. Check the following:
1. **Voltage Sag:** Ensure the HC-SR04 is getting a true 5V. Laptop USB ports running on power-saver mode can drop below 4.7V, blinding the sensor. Use a strong powered hub or battery output.
2. **Software Shield Gating:** The firmware includes a built-in `is_audio_playing` safety gate. While the speaker is active, the ultrasonic sensor is temporarily muted to avoid false readings caused by acoustic vibrations and pin noise.
3. **The Swap Test:** If you receive a flat `999` loop, swap your physical **Trig** and **Echo** jumper positions on the breadboard.
