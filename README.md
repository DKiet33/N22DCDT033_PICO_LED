# Voice-Activated LED Strip Controller (Raspberry Pi Pico 2)

An embedded C++ TinyML project that uses a **Raspberry Pi Pico 2 (RP2350)** and a microphone to control a WS2812/NeoPixel LED strip via voice commands.

This system runs **fully offline** on the microcontroller—no Wi-Fi, no cloud processing, no companion smartphone app. It listens for the keywords **"activate"** and **"cancel"** using a 1D Convolutional Neural Network trained on Edge Impulse.

---

## Features

* **100% Offline Keyword Spotting**: Local, continuous audio inference running on the RP2350.
* **Dual-Core Architecture**:
  * **Core 0**: Handles audio sampling (ADC + DMA) at 4 kHz and runs the Edge Impulse Machine Learning model.
  * **Core 1**: Dedicated to the LED State Machine and driving the NeoPixel strip via PIO (Programmable I/O) to ensure perfectly smooth animations without interrupting the ML inference.
* **14 Dynamic Lighting Modes**: Includes static colors, breathing, sequential wipes, fire/flame simulations, meteor rain, Cylon scanners, theater chases, sparkles, and dual-comets.
* **Inter-core Communication**: Uses the RP2350's hardware FIFO queue for thread-safe command passing between the ML engine and the LED controller.

---

## Hardware Requirements & Wiring

| Component                           | Specification                                 | Purpose                                      |
| ----------------------------------- | --------------------------------------------- | -------------------------------------------- |
| **Raspberry Pi Pico 2**       | RP2350, dual Cortex-M33 @ 150 MHz, 520 KB RAM | Main MCU — Core 0 runs ML, Core 1 runs LEDs |
| **Electret Microphone**       | Adafruit MAX9814 with AGC                     | Audio input with automatic gain control      |
| **LED Strip**                 | WS2812 / NeoPixel, any length                 | Output device driven via PIO at 800 kHz      |
| **5V Power Supply**           | Current ≥ (60 mA × number of LEDs)          | Powers LED strip independently from the Pico |
| **Breadboard + Jumper Wires** | Standard                                      | Prototyping connections                      |

### Wiring Diagram

```text
┌──────────────────────────────────────────────────┐ 
│                 Raspberry Pi Pico 2              │
│                                                  │
│  GP26 (ADC0) ◄──── OUT  [MAX9814 Microphone]     │
│  3V3(OUT)    ────► V+   [MAX9814]                │
│  GND         ────► GND  [MAX9814]                │
│                                                  │
│  GP7         ────► DIN  [WS2812 LED Strip]       │
│  GND         ────► GND  [WS2812 LED Strip]       │
│                                                  │
│  GP25        ──── Onboard LED (debug blinker)    │
└──────────────────────────────────────────────────┘

[Pico Micro USB]  ────► 3.3V power to Pico 2
[5V Power Supply V]  ────► V    [WS2812 LED Strip]
[5V Power Supply GND]  ────► GND  [WS2812 LED Strip] (shared with Pico GND)
```

> **WARNING:** **Do NOT power the LED strip from the Pico's VBUS/3V3.** A 60-LED strip at full brightness draws ≈3.6A. Use a separate 5V supply and **share a common ground** with the Pico.

---

## Software & Environment Requirements

| Tool                                 | Min Version           | Purpose                                                  |
| ------------------------------------ | --------------------- | -------------------------------------------------------- |
| **Raspberry Pi Pico SDK**      | 2.0+                  | RP2350 hardware abstraction                              |
| **GNU ARM Embedded Toolchain** | arm-none-eabi-gcc 10+ | C++ cross-compiler for Cortex-M33                        |
| **CMake**                      | 3.13+                 | Build system generator                                   |
| **NMake or Ninja**             | Any                   | Build backend (Windows)                                  |
| **Python 3**                   | 3.8+                  | WAV conversion scripts (`scipy`, `numpy`, `pydub`) |
| **Edge Impulse Studio**        | Free account          | Model training and C++ export                            |

### Installation Tutorial (Windows)

1. **Install CMake & Build Tools**

   - Download and install [CMake](https://cmake.org/download/). Make sure to select **"Add CMake to the system PATH"** during installation.
   - Install **Visual Studio Build Tools** (or full Visual Studio) and ensure the "Desktop development with C++" workload is selected. This provides `nmake`.
2. **Install GNU ARM Embedded Toolchain**

   - Download the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) for Windows.
   - Run the installer. On the final screen, **check the box** that says **"Add path to environment variable"**.
3. **Install Python 3**

   - Download from [python.org](https://www.python.org/downloads/).
   - **Check "Add Python to PATH"** at the very bottom of the installer before clicking Install.
   - Open a terminal and install the required packages:
     ```powershell
     pip install scipy numpy pydub
     ```
4. **Set Up the Raspberry Pi Pico SDK**

   - Open a terminal (like Git Bash or PowerShell).
   - Clone the SDK repository:
     ```powershell
     git clone -b master https://github.com/raspberrypi/pico-sdk.git
     cd pico-sdk
     git submodule update --init
     ```
   - Set the environment variable so CMake can find the SDK. Open PowerShell and run:
     ```powershell
     [Environment]::SetEnvironmentVariable("PICO_SDK_PATH", "C:\your\actual\path\to\pico-sdk", "User")
     ```
   - *(Be sure to replace the path above with exactly where you cloned the repo).* Restart your terminal afterward to ensure the new variable is loaded.

---

## Project Architecture & Flow

```text
[Your Voice]
     |
     v
[MAX9814 Microphone (AGC)]
     | Analog voltage signal
     v
[RP2350 ADC — GPIO 26 / Channel 0]
     | 12-bit digital samples @ ~4 kHz (CLOCK_DIV = 12000)
     | via DMA → capture_buf[1000]
     v
[Sliding Window Buffer — intermediate_buf[4000]]
     | Oldest 3000 samples wrapped forward + 1000 new samples appended
     v
[Normalization — float features[4000]]
     | Raw uint16 values → floats scaled to [-32766, +32766]
     v
[Edge Impulse run_classifier()]
     | DSP block (MFCC/MFE) extracts frequency features
     | Neural Network classifies audio window
     v
[Classification Result — confidence scores per label]
     | noise | other | activate | cancel
     v
[Threshold Check — thresh = 0.7]
     |
     +--[ix == 2 "activate" > 0.7 AND cooldown elapsed]──> model_result = 1
     |
     +--[ix == 3 "cancel" > 0.7]──────────────────────> model_result = 2
     |
     v
[Multicore FIFO — hardware queue between Core 0 and Core 1]
     | Core 0 pushes model_result (1 or 2)
     | Core 1 pops the value
     v
[LED State Machine — Core 1]
     |
     +--[val == 1, lights OFF]──> Turn ON, from state 0
     |
     +--[val == 1, lights ON] ──> Cycle to next state (state + 1) % NUM_STATES
     |
     +--[val == 2]──────────────> Turn OFF (clear strip)
     v
[Adafruit NeoPixel PIO Driver — GPIO 7]
     v
[WS2812 LED Strip]
```

---

## Step-by-Step Project Plan

### Phase 1 — Hardware Assembly

1. Wire the Pico 2, MAX9814, and WS2812 strip according to the diagram above.
2. Connect external 5V power to the LED strip and share the ground with the Pico.

### Phase 2 — Collect Training Data

There are two approaches to collecting your own custom training data:

**Approach A: Using `pico-daq` (Hardware Accurate)**
Uses the actual Pico and microphone you built. Build and flash the `pico-daq` firmware, record serial output, and convert the Base64 data to WAV files using the provided Python scripts. This guarantees a 4000 Hz sample rate.

**Approach B: Direct Recording in Edge Impulse (Fastest)**
Use your smartphone or computer microphone to record samples directly into the Edge Impulse Studio.
*Note: Phones and PCs record at 16,000+ Hz. The Pico runs at 4,000 Hz. You **MUST** ensure the "Frequency" is set to exactly `4000 Hz` when creating your Impulse.*

### Phase 3 — Train the ML Model in Edge Impulse

1. Create an Impulse: Window size 1000 ms, Window increase 250 ms, Frequency 4000 Hz.
2. Use Audio (MFE or MFCC) for processing and Classification (Keras) for learning.
3. Train the model and ensure ≥ 90% accuracy.
4. Go to **Deployment** → choose **C++ Library** → select **Quantized (int8)** and download the ZIP.
5. Extract the `edge-impulse-sdk`, `model-parameters`, and `tflite-model` folders into the `pico-light-voice/` project folder.

### Phase 4 — Customize the Firmware Code

In `main.cpp`:

* Verify `NSAMP` (1000), `INSIZE` (4000), `CLOCK_DIV` (12000), `CAPTURE_CHANNEL` (0), `LED_PIN` (25).
* Adjust the `thresh` (default 0.7) if necessary.
* Ensure your keyword indices (`ix == 2` for "activate", `ix == 3` for "cancel") match your Edge Impulse model labels.

In `lights.cpp`:

* Set `PIN` (7) and `NUM_LIGHTS` to match your actual LED count.

### Phase 5 — Build & Flash

1. Open a terminal (like **Developer PowerShell for VS**).
2. Navigate to the `pico-light-voice` build directory.
3. Run `cmake .. -G "NMake Makefiles" -DPICO_BOARD=pico2` (or Ninja).
4. Run `nmake` (or `ninja`).
5. Hold **BOOTSEL** on the Pico 2, connect USB, and drag `build/pico-voice.uf2` onto the `RPI-RP2` drive.
6. Monitor serial output (115200 baud) for classification probabilities.

---

## Technical Skills & Concepts (TinyML + DSP)

### ADC & DMA

The microphone's analog voltage is digitized by the ADC at ~4 kHz. DMA transfers these samples to RAM in the background, allowing simultaneous audio capture and ML inference without CPU blocking.

### Multicore Programming & FIFO

Core 0 runs the ML inference. Core 1 runs the LED state machine. They communicate via a hardware FIFO. Core 0 pushes a `1` (activate) or `2` (cancel), and Core 1 pops it to update the LED strip.

### Sliding Window & Normalization

Every 250ms, 1000 new samples are added and 1000 old samples are dropped. The raw ADC output is dynamically normalized to a float range of [-32766, +32766] to ensure consistent volume input for the neural network.

### WS2812 PIO Control

The RP2350's PIO state machines generate the precise nanosecond-level timing required by the WS2812 protocol, offloading this burden entirely from the CPU.

---

## Requirements & Constraints

* **FR-01/02**: Say "activate" to turn on the LED strip (State 0). Say it again to cycle through 14 modes.
* **FR-03**: Say "cancel" to turn off the strip.
* **FR-04**: Inference must complete within 250ms. The onboard LED (GP25) blinks to signal healthy inference loops.
* **NR-01**: Total RAM usage must not exceed 520 KB. Keep `NSAMP ≤ 1000` and `INSIZE ≤ 4000` (int8 quantization is critical).
* **NR-03**: Training WAV sample rate (4000 Hz) must exactly match firmware ADC sample rate.

---

## Common Gotchas & Troubleshooting

| Problem                                    | Cause                  | Fix                                                          |
| ------------------------------------------ | ---------------------- | ------------------------------------------------------------ |
| Model accuracy is poor on-device           | Sample rate mismatch   | Ensure `CLOCK_DIV = 12000` and training WAVs are 4000 Hz   |
| Build error: "Input frame size incorrect!" | `INSIZE` mismatch    | Make `INSIZE` match your model's expected input (4000)     |
| Pico hangs or crashes silently             | RAM exhaustion         | Keep `INSIZE ≤ 4000`; use quantized (int8) model          |
| LED doesn't blink during inference         | Inference takes >250ms | Model is too complex; reduce NN layers or use int8           |
| LED strip flickers / only first LED lights | Wrong NeoPixel config  | Check `NEO_GRB + NEO_KHZ800`; set `NUM_LIGHTS` correctly |

---

## Credits & Acknowledgements

This project was built upon the incredible open-source work of several individuals and organizations:

* **[Alex Wulff](https://github.com/AlexFWulff)**: The baseline architecture for capturing ADC audio via DMA and piping it into Edge Impulse was adapted from his **[awulff-pico-playground](https://github.com/AlexFWulff/awulff-pico-playground)** repository.
* **[Edge Impulse](https://www.edgeimpulse.com/)**: For providing the TinyML platform used to train, quantize (int8), and export the 1D-CNN keyword spotting model as a highly optimized C++ library.
* **[Raspberry Pi Foundation](https://github.com/raspberrypi/pico-sdk)**: For the fantastic `pico-sdk` which makes multicore processing, DMA, and PIO state machines highly accessible.
* **[Adafruit Industries](https://github.com/adafruit/Adafruit_NeoPixel)**: For the foundational NeoPixel protocol logic and standard libraries.
* **[Martin Kooij](https://github.com/martinkooij/pi-pico-adafruit-neopixels)**: For the excellent `pi-pico-adafruit-neopixels` port which adapted Adafruit's libraries to use the Raspberry Pi Pico PIO state machines seamlessly.
