# Requirements — Voice-Activated LED Strip Controller

This document lists all hardware, software, and knowledge requirements for this project.

---

## Hardware Requirements

| Component | Specification | Purpose | Status |
|-----------|--------------|---------|--------|
| **Raspberry Pi Pico 2** | RP2350, dual Cortex-M33 @ 150 MHz, 520 KB RAM | Main MCU — Core 0 runs ML, Core 1 runs LEDs | `[x]` |
| **Electret Microphone** | Adafruit MAX9814 with AGC | Audio input with automatic gain control | `[x]` |
| **LED Strip** | WS2812 / NeoPixel, any length | Output device driven via PIO at 800 kHz | `[x]` |
| **5V Power Supply** | Current ≥ (60 mA × number of LEDs) | Powers LED strip independently from the Pico | `[x]` |
| **Breadboard + Jumper Wires** | Standard | Prototyping connections | `[x]` |
| **Micro-USB Cable** | — | Power and firmware flashing | `[x]` |

### Wiring Pin Map

| Pico Pin | Connected To | Signal |
|----------|-------------|--------|
| GP26 (ADC0) | MAX9814 OUT | Analog audio input |
| 3V3(OUT) | MAX9814 VCC | Microphone power |
| GND | MAX9814 GND | Common ground |
| GP7 | WS2812 DIN | NeoPixel data signal (PIO) |
| GND | WS2812 GND | Shared ground with 5V supply |
| GP25 | — | Onboard LED — blinks once per inference cycle |
| External 5V | WS2812 VCC | LED power (**never** from Pico 3V3) |

---

## Software Requirements

| Tool | Min Version | Purpose |
|------|------------|---------|
| **Raspberry Pi Pico SDK** | 2.0+ | RP2350 hardware abstraction |
| **GNU ARM Embedded Toolchain** | arm-none-eabi-gcc 10+ | C++ cross-compiler for Cortex-M33 |
| **CMake** | 3.13+ | Build system generator |
| **NMake or Ninja** | Any | Build backend (Windows) |
| **Python 3** | 3.8+ | WAV conversion scripts |
| **pip: scipy, numpy, pydub** | Any | Required by `b64_float_to_wave.py` |
| **Edge Impulse Studio** | Free account | Model training and C++ export |
| **Serial terminal** | — | 115200 baud, 8N1 (PuTTY, Tera Term, etc.) |

### Environment Variables (Windows)

```powershell
$env:PICO_SDK_PATH = "C:\path\to\pico-sdk"
# arm-none-eabi-gcc must also be on PATH
```

---

## Firmware / Library Dependencies

| Folder | Source | Status |
|--------|--------|--------|
| `edge-impulse-sdk/` | Edge Impulse C++ export ZIP | `[x]` Re-extracted from v12 model ZIP |
| `model-parameters/` | Edge Impulse C++ export ZIP | `[x]` Re-extracted from v12 model ZIP |
| `tflite-model/` | Edge Impulse C++ export ZIP | `[x]` Re-extracted from v12 model ZIP |
| `pico_neopixels/` | Adafruit NeoPixel PIO port | `[x]` Already in workspace |
| `pico-sdk/` | Raspberry Pi Pico SDK | `[x]` Already in workspace |

> **IMPORTANT:** After extracting the EI ZIP, the bundled `CMakeLists.txt` will overwrite the project's custom one. Always restore the project's `CMakeLists.txt` immediately after extraction.

---

## Dataset Requirements

### Basic Labels (Phase 2A — ready)

| Label | Folder | Files present | Format |
|-------|--------|--------------|--------|
| `activate` | `dataset/activate/` | Ready | `activate.xx.wav` |
| `cancel` | `dataset/cancel/` | Ready | `cancel.xx.wav` |
| `noise` | `dataset/noise/` | Ready | `noise.xx.wav` |
| `other` | `dataset/other/` | Ready | `other.xx.wav` |



### Audio Sample Specifications

| Property | Required Value | Reason |
|----------|---------------|--------|
| Sample rate | **4000 Hz** | Must match `CLOCK_DIV = 12000` in firmware |
| Duration per sample | **~1 second** | Matches the model's 1000ms window |
| Format | WAV (PCM) | Required by Edge Impulse uploader |
| Minimum samples per label | **50+** | Adequate for 80/20 train/test split |

---

## Edge Impulse Model Requirements

| Parameter | Value | Notes |
|-----------|-------|-------|
| Window size | 1000 ms | = 4000 samples @ 4 kHz |
| Window increase | 250 ms | Matches `NSAMP = 1000` → 4 inferences/sec |
| Frequency | 4000 Hz | Must match firmware ADC rate |
| Processing block | MFCC or MFE | Standard for keyword spotting |
| Learning block | Classification (Keras) | — |
| Quantization | **int8** | Required to fit RP2350 RAM constraints |
| `EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE` | Must equal `INSIZE` (4000) | Firmware halts with error if mismatched |
| Target accuracy | ≥ 90% | On held-out test set before deploying. Current model achieved ~95.8% validation accuracy |

---

## Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | Say "activate" → LED strip turns on in State 0 (Default RGB flowing cycle) | High |
| FR-02 | Say "activate" again → LED cycles to next lighting mode (14 modes total) | High |
| FR-03 | Say "cancel" → LED strip turns off immediately regardless of state | High |
| FR-04 | Full inference must complete within 250ms per window | High |
| FR-05 | System runs fully standalone — no Wi-Fi, phone, or PC at runtime | High |
| FR-06 | Onboard LED (GP25) blinks once per inference cycle as health indicator | Medium |
| FR-07 | 1-second cooldown prevents "activate" from triggering twice per utterance | Medium |
| FR-08 | Confidence threshold set to `0.7` to minimize false positives | Medium |

---

## Non-Functional Requirements

| ID | Requirement |
|----|-------------|
| NR-01 | Total RAM usage must not exceed 520 KB. Keep `NSAMP ≤ 1000` and `INSIZE ≤ 4000` |
| NR-02 | LED strip must be powered from an external 5V supply, not from the Pico's 3V3 pin |
| NR-03 | Training WAV sample rate (4000 Hz) must exactly match firmware ADC sample rate |
| NR-04 | Edge Impulse model must be exported as int8-quantized C++ Library |
| NR-05 | Keyword label indices (`ix == N`) must match the alphabetical order of exported model labels |
