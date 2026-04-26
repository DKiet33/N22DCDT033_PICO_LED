# Voice-Activated LED Strip Controller — Raspberry Pi Pico 2

Build a hands-free, voice-controlled WS2812/NeoPixel LED strip using a Raspberry Pi Pico 2, a microphone with AGC, and an Edge Impulse keyword-spotting ML model. Say **"activate"** to turn on / cycle modes; say **"cancel"** to turn off.

---

## Project Architecture Overview

**Core 0 — Audio + ML**
`[Microphone (ADC Ch0, GP26)]` 
  └─(DMA @ 4 kHz)─> `[Sample Buffer]`
  └─> `[Feature Buffer (float, 4000 samples)]`
  └─> `[Edge Impulse Classifier]`
  └─("ACTIVATE / CANCEL")─> `[Multicore FIFO]`

**Core 1 — LED State Machine**
`[Multicore FIFO]`
  └─> `[State Machine]`
  └─> `[NeoPixel Strip (PIO)]`

---

## Phase 0 — Prerequisites & Environment Setup

### Software you need installed

| Tool | Purpose | Install Link |
|------|---------|-------------|
| **Pico SDK 2.x** | RP2350 HAL + build system | `git clone -b master https://github.com/raspberrypi/pico-sdk.git; cd pico-sdk; git submodule update --init` |
| **GNU ARM Embedded Toolchain** | Cross-compiler for Cortex-M33 | [arm.com/downloads](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) |
| **CMake ≥ 3.13** | Build generator | [cmake.org/install](https://cmake.org/install/) |
| **Ninja or NMake** | Build backend (Windows) | Bundled with VS Build Tools or standalone |
| **Python 3 + pip** | For WAV conversion scripts | `pip install scipy numpy pydub` |
| **Edge Impulse Studio account** | Train and export your ML model | [studio.edgeimpulse.com](https://studio.edgeimpulse.com) |
| **A serial terminal** | PuTTY / Tera Term / Arduino Serial Monitor | Any 115200 baud, 8N1 |

### Environment variables (Windows)

```powershell
# Set these in your system or shell before building
$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"
# Ensure arm-none-eabi-gcc is on your PATH
```

> **IMPORTANT:** Your `CMakeLists.txt` already sets `set(PICO_BOARD pico2)` — this targets the RP2350. Make sure your Pico SDK version supports RP2350. SDK 2.0+ is required.

---

## Phase 1 — Hardware Assembly

### Parts list

| Part | Quantity | Notes |
|------|----------|-------|
| Raspberry Pi Pico 2 (RP2350) | 1 | Already on hand per your CMakeLists |
| Adafruit MAX9814 Electret Mic (AGC) | 1 | [Amazon link](https://amzn.to/3Iiy597) — has automatic gain control |
| WS2812 / NeoPixel LED strip | 1 | Any length; set `NUM_LIGHTS` to match |
| 5V power supply | 1 | Must handle strip current (≈60 mA per LED at full white) |
| Breadboard + jumper wires | 1 set | — |

### Wiring diagram

```
┌──────────────────────────────────────────────────┐ 
│                 Raspberry Pi Pico 2              │
│                                                  │
│  GP26 (ADC0) ◄──── OUT  [MAX9814 Microphone]     │
│  3V3(OUT)    ────► VCC  [MAX9814]                │
│  GND         ────► GND  [MAX9814]                │
│                                                  │
│  GP7         ────► DIN  [WS2812 LED Strip]       │
│  GND         ────► GND  [WS2812 LED Strip]       │
│                                                  │
│  GP25        ──── Onboard LED (debug blinker)    │
└──────────────────────────────────────────────────┘

[5V Power Supply] ────► VCC  [WS2812 LED Strip]
[5V Power Supply] ────► GND  [WS2812 LED Strip] (shared with Pico GND)
```

> **WARNING:** **Do NOT power the LED strip from the Pico's VBUS/3V3.** A 60-LED strip at full brightness draws ≈3.6A. Use a separate 5V supply and **share a common ground** with the Pico.

> **TIP:** The MAX9814 AGC gain can be set by connecting the GAIN pin to VCC (40dB), leaving it floating (50dB), or connecting to GND (60dB). For voice commands at arm's length, 50dB (floating) works well.

---

## Phase 2 — Collect Training Data

> **UPDATE:** We are using an iterative approach! The initial 4-label dataset (`activate`, `cancel`, `noise`, `other`) is fully collected, formatted, and ready in the `dataset/` directory. You can skip directly to **Step 2.3** to upload this data and train the basic model.

There are two approaches to collecting your own custom training data.

### Approach A: Using `pico-daq` (Hardware Accurate)

This approach uses the actual Pico and microphone you built, ensuring the audio characteristics perfectly match what the model will hear in production.

#### A.1 Build & flash the data collection firmware

```powershell
cd c:\CODE\HEDIEUHANH_NHUNG\PICO_LEDSTRIP\awulff-pico-playground-main\pico-daq
mkdir build
cd build
cmake .. -G "NMake Makefiles"
nmake
```

This produces `pico_daq.uf2`. Flash it:
1. Hold **BOOTSEL** on the Pico, plug in USB
2. Drag `pico_daq.uf2` to the `RPI-RP2` drive

#### A.2 Record audio samples

1. Open a serial terminal at **115200 baud**
2. The Pico continuously streams base64-encoded audio over USB serial
3. For **each keyword** ("activate", "cancel", "noise", etc.):
   - Record ~50+ one-second samples each.
   - **Speech Variants to capture:** Normal, fast, slow, high pitch, low pitch, and from different distances (30cm, 1m, 2m).
   - Save the serial output to a text file (e.g., `activate-1.txt`, `cancel-1.txt`, `noise-1.txt`).

#### A.3 Convert base64 → WAV

Use the provided Python script (adapt the paths for your system):

```python
# c:\CODE\...\pico-daq\py\b64_float_to_wave.py
# Change infile/outfile paths, then run:
python b64_float_to_wave.py
```

> **IMPORTANT:** The script writes WAV at **4000 Hz** sample rate (`scipy.io.wavfile.write(wav_io, 4000, data)`). This **must match** the ADC sample rate used in inferencing. Your `CLOCK_DIV = 12000` gives ≈ 4 kHz.

---

### Approach B: Direct Recording in Edge Impulse (Fastest)

You can use your smartphone or computer microphone to record samples directly into the Edge Impulse Studio.

#### B.1 Connect a Device
1. Go to [studio.edgeimpulse.com](https://studio.edgeimpulse.com) → **Data acquisition**.
2. Click **Connect a device** and choose **Use your mobile phone** or **Use your computer**.
3. Follow the QR code or on-screen instructions to grant microphone access.

#### B.2 Record Samples
1. Set the label (e.g., `activate`, `cancel`, `noise`).
2. Record ~1-second samples of you speaking the keyword (at least 50+ per label).
> **WARNING:** Phones and PCs record at 16,000+ Hz. The Pico runs at 4,000 Hz. You **MUST** ensure the "Frequency" is set to exactly `4000 Hz` when you create your Impulse in Phase 3 so Edge Impulse downsamples the audio!

---

### 2.3 Upload & Prepare Dataset (If using Approach A or provided dataset)

1. Go to [studio.edgeimpulse.com](https://studio.edgeimpulse.com) → create a new project
2. **Data acquisition** → Click the "Upload data" icon
3. Select your `.wav` files and assign their labels (`activate`, `cancel`, `noise`, `other`)
4. Ensure an approximate 80/20 Train/Test split in your dataset

---

## Phase 3 — Train the ML Model in Edge Impulse

### 3.1 Create Impulse

| Block | Setting | Value |
|-------|---------|-------|
| **Time series data** | Window size | 1000 ms |
| | Window increase | 250 ms |
| | Frequency | 4000 Hz |
| **Processing block** | Type | Audio (MFE or MFCC) |
| **Learning block** | Type | Classification (Keras) |

### 3.2 Generate features

- Go to the **MFE** or **MFCC** page → Generate features
- Verify the feature explorer shows good cluster separation between "activate", "cancel", and noise

### 3.3 Train the classifier

- Use the default NN architecture (or simplify for Pico RAM constraints)
- Train for 100+ epochs
- Target accuracy: ≥ 90%

### 3.4 Deploy as C++ Library

1. Go to **Deployment** → choose **C++ Library**
2. Select **Quantized (int8)** to save RAM on the Pico
3. Download the ZIP

### 3.5 Extract into your project

Unzip the deployment and copy these three folders into `pico-light-voice/`:

```
pico-light-voice/
├── edge-impulse-sdk/    ← from ZIP
├── model-parameters/    ← from ZIP
├── tflite-model/        ← from ZIP
├── source/
│   ├── main.cpp
│   ├── lights.cpp
│   └── lights.h
├── pico_neopixels/
├── CMakeLists.txt
└── pico_sdk_import.cmake
```

> **CAUTION:** The `edge-impulse-sdk` folder is large (many hundreds of files). Make sure you copy the **entire** folder structure. Missing files cause cryptic build errors.

---

## Phase 4 — Customize the Firmware Code

### 4.1 Key parameters in `main.cpp`

| Parameter | Current Value | What it controls |
|-----------|--------------|------------------|
| `NSAMP` | 1000 | Samples per inference window (1000 @ 4kHz = 250ms) |
| `INSIZE` | 4000 | Total model input size (4000 @ 4kHz = 1 second) |
| `CLOCK_DIV` | 12000 | ADC clock divider → ~4 kHz sample rate |
| `CAPTURE_CHANNEL` | 0 | ADC channel (GPIO 26) |
| `LED_PIN` | 25 | Onboard LED for debug blinking |
| `COOLDOWN_US` | 1000000 | 1-second cooldown between "activate" triggers |
| `thresh` | 0.7 | Confidence threshold for keyword detection |

### 4.2 Match keyword indices

```cpp
// In main.cpp, lines 148-163:
// ix == 2 → "activate" keyword
// ix == 3 → "cancel" keyword
```

> **IMPORTANT:** **You MUST check your model's label order.** In Edge Impulse Studio → go to your impulse → look at the label indices. They are alphabetical by default, so typically:
> - `ix == 0` → `_noise` (or `noise`)
> - `ix == 1` → `_other`
> - `ix == 2` → `activate`
> - `ix == 3` → `cancel`
>
> If your labels are different, update the `if (ix == ...)` conditions accordingly.

### 4.3 Customize `lights.cpp`

| Parameter | Current Value | What to change |
|-----------|--------------|----------------|
| `PIN` | 7 | GPIO pin connected to the LED strip's DIN |
| `NUM_STATES` | 14 | Number of lighting modes to cycle through |
| `NUM_LIGHTS` | 24 | **Set this to your actual LED count** |

The current lighting states are:
- **State 0** — Default RGB flowing cycle
- **State 1** — RGB flowing cycle in reverse
- **State 2** — Breathing colors (7 colors of the rainbow)
- **State 3** — Flashing colors (7 colors of the rainbow)
- **State 4** — Sequential Rainbow Wipe (Forward)
- **State 5** — Sequential Rainbow Wipe (Reverse)
- **State 6** — Static Random Color (Changes every time triggered)
- **State 7** — Fire / Flame
- **State 8** — Meteor Rain
- **State 9** — Cylon / Larson Scanner
- **State 10** — Theater Chase
- **State 11** — Sparkle / Strobe
- **State 12** — Color Twinkle
- **State 13** — Comets

---

## Phase 5 — Build & Flash the Final Firmware

### 5.1 Build

```powershell
cd c:\CODE\HEDIEUHANH_NHUNG\PICO_LEDSTRIP\awulff-pico-playground-main\pico-light-voice
mkdir build
cd build
cmake .. -G "NMake Makefiles"
nmake
```

> **NOTE:** The build will take several minutes the first time due to compiling the entire Edge Impulse SDK and TFLite Micro. Subsequent builds are incremental.

### 5.2 Flash

1. Hold **BOOTSEL** on the Pico 2, connect USB
2. Drag `build/pico-voice.uf2` onto the `RPI-RP2` drive
3. The Pico reboots and starts running

### 5.3 Monitor serial output

Open a serial terminal (115200 baud). You should see:
- Classification probabilities printed every ~250ms (4× per second)
- `activate` or `cancel` printed when a keyword is detected above the threshold
- `Turning lights on (activate)`, `Incrementing state to X`, `Turning lights off (cancel)` from Core 1

---

## Common Gotchas & Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| Model accuracy is poor on-device | Sample rate mismatch between training WAVs (4000 Hz) and ADC | Ensure `CLOCK_DIV = 12000` and WAVs are written at 4000 Hz |
| Build error: "Input frame size incorrect!" | `INSIZE` ≠ `EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE` | Make `INSIZE` match your model's expected input (usually = sample rate × window length) |
| Pico hangs or crashes silently | RAM exhaustion — `NSAMP` or `INSIZE` too large | Keep `NSAMP` ≤ 1000 and `INSIZE` ≤ 4000; use quantized (int8) model |
| LED doesn't blink during inference | Inference takes longer than the sampling window | Model is too complex; reduce NN layers or use int8 quantization |
| Lights don't respond | FIFO flooding or wrong keyword indices | Rate-limit FIFO writes (the code already does this); verify `ix == 2`/`ix == 3` |
| LED strip flickers or only first LED lights | Wrong NeoPixel type or `NUM_LIGHTS` mismatch | Check `NEO_GRB + NEO_KHZ800`; set `NUM_LIGHTS` correctly |
| `uint16_t` vs `float` type mismatch | Passing raw ADC buffer to model | The code normalizes to `float` in the feature buffer — don't skip this step |

---

## Verification Plan

### Automated Tests
- After Phase 2: Verify WAV files play correctly and are at 4 kHz sample rate
- After Phase 5: Monitor serial output to confirm classification scores are printed 4×/sec

### Manual Verification
- After Phase 1: Verify microphone readings via the DAQ firmware serial output
- After Phase 3: Check Edge Impulse model accuracy ≥ 90% on test set
- After Phase 5: Say "activate" → lights turn on; say "cancel" → lights turn off; say "activate" again → lights cycle to next mode
- Confirm onboard LED (GP25) blinks during each inference cycle (visual confirmation that inference completes before next sample window)

---

## Execution Order Summary

1. `[x]` **Phase 0:** Install SDK, Toolchain, CMake
      ↓
2. `[x]` **Phase 1:** Wire up circuit
      ↓
3. `[x]` **Phase 2:** Build pico-daq, collect audio samples (Basic labels)
      ↓
4. `[x]` **Phase 3:** Train model in Edge Impulse Studio
      ↓
5. `[x]` **Phase 4:** Export model, customize main.cpp & lights.cpp
      ↓
6. `[x]` **Phase 5:** Build & flash pico-voice, test!
      ↓
   🎉 **Done!**
