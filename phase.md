# Project Phases — Voice-Activated LED Strip Controller

This document tracks the status of each project phase and what is needed to advance.

---

## Status Legend

| Symbol | Meaning |
|--------|---------|
| `[ ]` | Not started |
| `[/]` | In progress / partially complete |
| `[x]` | Complete |
| `[-]` | Blocked — waiting on hardware or a previous phase |

---

## Phase 0 — Prerequisites & Environment Setup `[x]`

> **Status:** Complete. All software tools, SDKs, and compilers are verified and ready for building.

### Tasks
- `[x]` Install **Pico SDK 2.x** and set `PICO_SDK_PATH` environment variable
- `[x]` Install **GNU ARM Embedded Toolchain** (`arm-none-eabi-gcc`) and add to PATH
- `[x]` Install **CMake ≥ 3.13**
- `[x]` Install a build backend (**NMake** via Visual Studio Build Tools, or **Ninja**)
- `[x]` Install **Python 3** with `scipy`, `numpy`, and `pydub` packages
- `[x]` Create an **Edge Impulse Studio** account at [studio.edgeimpulse.com](https://studio.edgeimpulse.com)
- `[x]` Confirm the build tools work: run `arm-none-eabi-gcc --version` and `cmake --version`

---

## Phase 1 — Hardware Assembly `[x]`

> **Status:** Complete.

### Hardware needed
- `[x]` Raspberry Pi Pico 2 (RP2350)
- `[x]` Adafruit MAX9814 Electret Microphone with AGC
- `[x]` WS2812 / NeoPixel LED strip (confirm LED count for `NUM_LIGHTS`)
- `[x]` 5V power supply (capacity depends on strip length)
- `[x]` Breadboard and jumper wires

### Wiring tasks
- `[x]` Connect MAX9814 OUT → GP26, VCC → 3V3, GND → GND
- `[x]` Connect LED strip DIN → GP7, GND → GND (shared with Pico GND)
- `[x]` Connect LED strip VCC → 5V external power supply
- `[x]` Verify microphone output using pico-daq firmware over serial

---

## Phase 2 — Collect Training Data `[x]`

> **Status:** Complete. `activate`, `cancel`, `noise`, and `other` samples collected, uploaded, and used to train the active model.

### Stage A — Basic labels
- `[x]` Collect `activate` samples → Edge Impulse Studio
- `[x]` Collect `cancel` samples → Edge Impulse Studio
- `[x]` Collect `noise` samples → Edge Impulse Studio
- `[x]` Collect `other` samples → Edge Impulse Studio
- `[x]` Upload all samples and train the model (proceed to Phase 3)


---

## Phase 3 — Train the ML Model in Edge Impulse `[x]`

> **Status:** Complete. Model trained with `activate`, `cancel`, `noise`, `other` labels using 1D-CNN classifier. Deployed as C++ Library (int8 quantized).

### Tasks
- `[x]` Create Edge Impulse project with `activate`, `cancel`, `noise`, `other` labels
- `[x]` Configure Impulse: 1000ms window, 250ms window increase, 4000 Hz sample rate
- `[x]` Add MFCC or MFE processing block
- `[x]` Generate features and verify cluster separation in feature explorer
- `[x]` Train NN classifier (target accuracy ≥ 90%)
- `[x]` Verify confusion matrix on test set (~95.8% validation accuracy achieved)
- `[x]` Deploy as **C++ Library** (Quantized int8)
- `[x]` Download ZIP

---

## Phase 4 — Integrate & Customize Firmware `[/]`

> **Status:** Complete. Firmware source code (`main.cpp`, `lights.cpp`) is fully written and customized. New EI deployment (v12) files have been extracted.

### Tasks
- `[x]` Write `main.cpp` — inference loop, `activate`/`cancel` keyword detection, FIFO signalling
- `[x]` Write `lights.cpp` — 14-state LED animation state machine with interrupt capability
- `[x]` Set `NUM_LIGHTS = 24` in `lights.cpp`
- `[x]` Set `thresh = 0.9` in `main.cpp` for confidence threshold
- `[x]` Write `CMakeLists.txt` with `ei_sdk` static library architecture
- `[x]` Extract new Edge Impulse C++ deployment ZIP into `pico-light-voice/`
  - Drop `edge-impulse-sdk/`, `model-parameters/`, `tflite-model/` into the project folder
  - **Note:** The EI ZIP will overwrite `CMakeLists.txt` — restore it from the project after extracting

---

## Phase 5 — Build & Flash Firmware `[/]`

> **Status:** Ready. New EI deployment files (v12) are in the project folder. You can now build.

### Build sequence (Developer PowerShell)
```powershell
cd C:\CODE\HEDIEUHANH_NHUNG\PICO_LEDSTRIP\awulff-pico-playground-main\pico-light-voice
mkdir build
cd build
cmake .. -G "NMake Makefiles" -DPICO_BOARD=pico2
nmake
```

> **CAUTION:** After extracting the EI ZIP, restore the custom `CMakeLists.txt`. The EI ZIP ships with a minimal one that will overwrite it.

### Tasks
- `[x]` Extract new EI deployment ZIP into `pico-light-voice/` (Phase 4 prerequisite)
- `[x]` Restore `CMakeLists.txt` after ZIP extraction
- `[ ]` Run cmake and nmake — confirm `pico-voice.uf2` is generated without errors
- `[ ]` Hold BOOTSEL, connect Pico, drag `.uf2` to `RPI-RP2` drive
- `[ ]` Open serial terminal at 115200 baud and confirm `[Inference]` scores are printing
- `[ ]` Say "activate" → LED turns on (State 0: rainbow cycle)
- `[ ]` Say "activate" again → LED cycles through all 14 states
- `[ ]` Say "cancel" → LED strip turns off immediately
- `[ ]` Confirm onboard LED (GP25) blinks during each inference cycle (no data loss)

---

## Phase 2B — Expand or Retrain Model (Future) `[ ]`

> **Status:** Not started. Deferred until the basic `up`/`down` pipeline is fully validated end-to-end.

### Tasks
- `[ ]` Re-record `activate` and `cancel` samples if model accuracy on-device is poor
- `[ ]` Retrain Edge Impulse model and re-export C++ Library
- `[ ]` Update `main.cpp` keyword matching for any new labels
- `[ ]` Rebuild and reflash firmware
