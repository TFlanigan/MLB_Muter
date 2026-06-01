# ESP32-CAM Commercial Detection with Edge Impulse

This project uses an ESP32-CAM and an IR emitter to detect TV commercials and toggle mute on a TV. The device captures TV frames, classifies them with an Edge Impulse model, and sends IR mute/unmute signals when commercials begin and end.

## Quick Start

1. Install PlatformIO.
2. Configure `src/data_collection.cpp` with WiFi and Edge Impulse credentials.
3. Upload the data collection sketch.
4. Collect labeled commercial and program images.
5. Train an Edge Impulse image classification model.
6. Deploy the model and update `src/main.cpp`.
7. Build and upload the main firmware.

## Requirements

- VS Code with PlatformIO extension, or PlatformIO Core CLI
- ESP32-CAM board
- IR emitter connected to the ESP32-CAM
- Edge Impulse account for model training

## Project Setup

### 1. Install PlatformIO

If you use VS Code, install the PlatformIO extension.

If you need the CLI directly, use the local install:

```bash
~/.platformio/penv/bin/platformio
```

If `pio` is not on your PATH, prepend the full path in commands.

### 2. Install Edge Impulse CLI (optional)

If you want local Edge Impulse upload support:

```bash
npm install -g edge-impulse-cli
```

### 3. Configure the Data Collection Sketch

Open `src/data_collection.cpp` and update these values:

```cpp
const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";
const char* EI_API_KEY = "ei_...";
const char* EI_PROJECT_ID = "your-project-id";
```

Save the file after editing.

## Build and Upload

This project has two separate firmware images that you can switch between:

### Option 1: Commercial Detection & Muting (Default)

Build and upload the main commercial detection firmware:

```bash
~/.platformio/penv/bin/platformio run --target upload
```

### Option 2: Data Collection for Edge Impulse

Build and upload the data collection firmware:

```bash
~/.platformio/penv/bin/platformio run -e data_collection --target upload
```

Both commands assume the ESP32-CAM is connected to your computer and will auto-detect the port.

## Data Collection

First, upload the data collection firmware to your ESP32-CAM:

```bash
~/.platformio/penv/bin/platformio run -e data_collection --target upload
```

Then:

> Note: these serial commands are implemented in the data collection sketch at `data_collection/data_collection.cpp`, not in `src/main.cpp`.

1. Open Serial Monitor at `115200` baud.
2. Point the camera at your TV.
3. Capture labeled frames:
   - `c` — commercial image
   - `p` — program image
   - `t` — test network/Edge Impulse connection
4. Collect many examples of both classes.
5. Check [Edge Impulse Studio](https://studio.edgeimpulse.com) to verify images arrive with correct labels.

## Train the Model in Edge Impulse

1. Open [Edge Impulse Studio](https://studio.edgeimpulse.com).
2. Create a project for this device.
3. Verify the uploaded images are labeled correctly.
4. In Impulse design:
   - add an Image block set to `96x96 RGB`
   - add a Classification block
5. Generate features and train the model.
6. Use a small model fit for ESP32, such as MobileNetV1 0.25.
7. Evaluate accuracy and retrain if needed.

## Deploy the Model

1. In Edge Impulse, go to Deployment → C++ library.
2. Download the ZIP archive.
3. Extract it into `src/edge-impulse-sdk/`.
4. Update `src/main.cpp` to include the generated model and inference code.

## Build and Upload the Main App

1. Confirm `src/main.cpp` contains the main commercial detection and IR logic.
2. Build and upload:

```bash
~/.platformio/penv/bin/platformio run --target upload
```

Or if `pio` is available:

```bash
pio run --target upload
```

## Usage

- Power the ESP32-CAM and point it at the TV screen.
- Aim the IR emitter at the TV’s IR receiver.
- The device will classify frames and toggle mute when commercials are detected.
- Use the serial console to monitor state and debug messages.

## File Structure

```text
MLB_Muter/
├── platformio.ini
├── src/
│   ├── main.cpp              # Main commercial detection firmware
│   ├── data_collection.cpp   # Data collection sketch for Edge Impulse
│   └── edge-impulse-sdk/     # Edge Impulse library and model files
├── commercial/               # Optional local training data
├── program/                  # Optional local training data
└── train_commercial_detector.py  # Optional helper script
```

## Troubleshooting

- `pio: command not found`
  - Use `~/.platformio/penv/bin/platformio` or install PlatformIO.
- `python3 -m platformio: No module named platformio`
  - Use the PlatformIO CLI from `.platformio/penv/bin`.
- Duplicate `setup()` / `loop()` build errors
  - Ensure only one Arduino sketch entry point exists in `src/`.
- WiFi or Edge Impulse upload failures
  - Check `WIFI_SSID`, `WIFI_PASS`, `EI_API_KEY`, and `EI_PROJECT_ID`.
- IR not working
  - Verify the correct IR code and emitter orientation.
- Low accuracy
  - Collect more labeled samples and retrain.

## Notes

- The IR mute code may need to be adjusted for your specific TV remote.
- Keep the camera stable and maintain consistent lighting.
- If the app is still using an old version of the README, make sure you opened the root `README.md` in the project folder rather than `test/README`.
