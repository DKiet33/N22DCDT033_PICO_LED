# Project Flow — Voice-Activated LED Strip Controller

This document describes the end-to-end data and signal flow of the system, from microphone input to LED output.

---

## System-Level Flow

```
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

## Core 0 — Audio + Inference Loop (main.cpp)

Runs continuously. Each loop iteration covers one inference window (~250ms).

```
Loop ON
  |
  1. Configure DMA to capture next NSAMP (1000) samples into capture_buf
  |
  2. Turn ON onboard LED (GP25) — signals inference is running
  |
  3. Normalize intermediate_buf → float features[]
     - Find min/max across the 4000-sample window
     - Scale: val = ((raw - min) / (max - min)) * 2 - 1, then * 32766
  |
  4. Call run_classifier() — invokes DSP + Neural Network
  |
  5. Check results against threshold (0.7):
     - "activate" detected → model_result = 1
     - "cancel" detected   → model_result = 2
  |
  6. Turn OFF onboard LED (GP25)
  |
  7. Wait for DMA to finish capturing NSAMP new samples
  |
  8. Slide the window:
     - Shift intermediate_buf left by NSAMP (drop oldest 1000 samples)
     - Append new capture_buf samples at the end
  |
Loop back to step 1
```

> **Onboard LED behaviour:** If the LED never turns OFF (stays solid), inference is taking longer than 250ms and audio is being lost between windows. Use a simpler model or int8 quantization to fix this.

---

## Core 1 — LED State Machine (lights.cpp)

Runs concurrently with Core 0. Polls the FIFO and drives the LED strip.

```
Startup
  |
  1. Initialize NeoPixel strip (NUM_LIGHTS = 60, PIN = 7, GRB @ 800kHz)
  |
  2. Push 0 to FIFO to signal Core 0 that Core 1 is ready
  |
  State: lights_on = false, state = 0

Main loop:
  |
  1. Poll FIFO for new command from Core 0:
     |
     +-- val == 2 → set lights_on = false
     |
     +-- val == 1, lights currently OFF → set lights_on = true (ON state 0)
     |
     +-- val == 1, lights currently ON  → state = (state + 1) % NUM_STATES
  |
  2. Render current state:
     |
     +-- lights_on == false → clear strip, off 200ms
     |
     +-- state == 0 → Default RGB flowing cycle
     |
     +-- state == 1 → RGB flowing cycle in reverse
     |
     +-- state == 2 → Breathing colors
     |
     +-- state == 3 → Flashing colors
     |
     +-- state == 4 → Sequential Rainbow Wipe (Forward)
     |
     +-- state == 5 → Sequential Rainbow Wipe (Reverse)
     |
     +-- state == 6 → Static Random Color
     |
     +-- state == 7 → Fire / Flame
     |
     +-- state == 8 → Meteor Rain
     |
     +-- state == 9 → Cylon / Larson Scanner
     |
     +-- state == 10 → Theater Chase
     |
     +-- state == 11 → Sparkle / Strobe
     |
     +-- state == 12 → Color Twinkle
     |
     +-- state == 13 → Comets
  |
  Loop back to step 1
```

---

## Intercore Communication Protocol

| Value sent via FIFO | Direction | Meaning |
|---------------------|-----------|---------|
| `0` | Core 1 → Core 0 | "I am ready / I have processed your command" |
| `1` | Core 0 → Core 1 | "activate" keyword detected — turn on or cycle state |
| `2` | Core 0 → Core 1 | "cancel" keyword detected — turn off lights |

> Core 0 only sends a command if Core 1 has already pushed `0` back (i.e., `multicore_fifo_rvalid()` is true). This prevents FIFO flooding.

---

## Data Collection Flows (Phase 2)

### Approach A: Hardware Accurate (pico-daq)

```
[Your Voice]
     |
     v
[MAX9814 Microphone]
     |
     v
[ADC + DMA — pico_daq.cpp]
     | Captures NSAMP = 10000 samples (~2.5 sec @ 4 kHz) per chunk
     v
[Normalize → float array]
     | Scale uint16 → float in [-1.0, +1.0]
     v
[Base64 Encode]
     | Binary float data → printable ASCII characters
     v
[USB Serial Output — 115200 baud]
     | Piped to your computer and saved as .txt files
     v
[b64_float_to_wave.py]
     | Decode Base64 → float array → WAV file @ 4000 Hz
     v
[dataset/activate/*.wav, dataset/cancel/*.wav, ...]
     v
[Upload to Edge Impulse Studio]
     v
[Train model → Export C++ Library]
     v
[Drop into pico-light-voice project and build]
```

### Approach B: Fastest (Direct Edge Impulse)

```
[Your Voice]
     |
     v
[Smartphone / Computer Microphone]
     |
     v
[Edge Impulse Studio Web Interface]
     |
     v
[Record 1-second samples directly via browser]
     |
     v
[Downsample to 4000 Hz in Impulse Design]
     |
     v
[Train model → Export C++ Library]
     |
     v
[Drop into pico-light-voice project and build]
```
