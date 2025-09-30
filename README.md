# ESP32_max98735a_I2S
# ESP32 Bluetooth Money Announcer (I2S MAX98357A)

This project uses the **ESP32** to receive a Serial string via **Bluetooth**, extract the monetary amount, convert that number into a Vietnamese audio sequence, and play it through a speaker via the **I2S amplifier module (MAX98357A)**.

---

## 1. Hardware Requirements

| Component         | Description                                                                 |
|-------------------|-----------------------------------------------------------------------------|
| Microcontroller   | ESP32 Dev Kit (Any standard version).                                       |
| Audio Module      | I2S Audio Amplifier Module **MAX98357A** (not MAX98735A).                   |
| Speaker           | 4 Ohm 3W speaker (or equivalent). Connects directly to the MAX98357A.       |

---

## 2. Wiring Diagram

The code uses **GPIO 25, 26, 27** pins for the I2S interface.

| MAX98357A Pin | ESP32 Pin | I2S Pin Name | Function/Purpose                                                                 |
|---------------|-----------|--------------|---------------------------------------------------------------------------------|
| DIN (Data In) | GPIO 25   | I2S DOUT     | Audio Data: Transfers digital PCM data from ESP32 to the amplifier.             |
| BCLK          | GPIO 26   | I2S BCLK     | Bit Clock: Synchronization clock pulse for each data bit.                       |
| LRC/WS        | GPIO 27   | I2S LRC      | Word Select: Defines data for Left/Right channel (Mono mode in code).           |
| SD            | 3.3V/VCC  | N/A          | Shutdown pin: **Must be HIGH** for chip to operate.                             |
| VCC           | 5V/3.3V   | N/A          | Power supply for the MAX98357A.                                                 |
| GND           | GND       | N/A          | Common ground.                                                                  |
| OUT+, OUT-    | Speaker   | N/A          | Speaker connection (4 Ohm 3W).                                                  |

---

## 3. Uploading Code and Data

The project requires two components to be uploaded:

1. **Code**  
2. **Audio Data (LittleFS)**  

### A. Preparing Audio Files (LittleFS)

- **Step 1:** Create a `data` folder in the same directory as your `.ino` or `.cpp` file.  
- **Step 2:** Place all `.wav` files in the `data` folder.  
  - File format: `/XXX_G711.org_.wav`  
  - Example: `/021_G711.org_.wav`  
  - `XXX` runs from **001 → 021**.  
- **Step 3:** Upload Data to ESP32:  

| Development Environment | Command/Action                                                                 |
|--------------------------|--------------------------------------------------------------------------------|
| Arduino IDE              | Tools → ESP32 Sketch Data Upload                                               |
| VS Code (PlatformIO)     | `Ctrl + Shift + P` → Upload LittleFS Filesystem Image                          |

This will format and upload all files inside the `data` folder to ESP32 Flash.

---

### B. Uploading Code

1. Select correct **Board (ESP32)** and **Port** in your IDE.  
2. Compile & upload the code.  

- **Arduino IDE:** Sketch → Upload (or `Ctrl + U`)  
- **VS Code (PlatformIO):** `Ctrl + Alt + U` or click Upload button  

---

## 4. Usage Instructions

### A. Connection
- After boot, the ESP32 will create a Bluetooth Serial connection:  
  **`ESP32_BT_VND_Reader`**  
- Use any Bluetooth Serial Terminal app on your phone/PC to connect.

### B. Sending Commands
- Send a string terminated by newline `\n`.  
- The code listens for `tien:` or money-related keywords.

#### Example Commands

| Command Sent                   | Expected Audio Output (Vietnamese)                                  |
|--------------------------------|----------------------------------------------------------------------|
| `tien:12345`                   | Bạn đã nhận được **mười hai nghìn ba trăm bốn mươi lăm đồng**        |
| `So tien GD: 500000 dong`      | Bạn đã nhận được **năm trăm nghìn đồng**                             |

---

## 5. Notes

- Ensure **.wav files are properly cut & compressed** (G711 format recommended).  
- Keep **SD pin HIGH** to avoid mute.  
- The project is currently configured for **Mono audio**.  

---

## 6. File Structure Example


```
project/
├── loamomo_stable_i2s.cpp
├── data/
│ ├── 001_G711.org_.wav
│ ├── 002_G711.org_.wav
│ ├── ...
│ └── 021_G711.org_.wav
└── README.md
```
