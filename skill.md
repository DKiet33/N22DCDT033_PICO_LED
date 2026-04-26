# Skills Reference — Voice-Activated LED Strip Controller

This document summarizes the key technical skills and concepts involved in this project, organized by domain.

---

## 1. Embedded C++ Programming (Raspberry Pi Pico)

### ADC & DMA
- **ADC (Analog-to-Digital Converter):** Converts the analog voltage from the microphone into a 12-bit digital number. Configured via `adc_init()`, `adc_gpio_init()`, and `adc_set_clkdiv()`.
- **DMA (Direct Memory Access):** Transfers ADC samples to RAM automatically in the background, without any CPU involvement. Configured via `dma_claim_unused_channel()` and `dma_channel_configure()`. This is what allows simultaneous audio capture and ML inference.
- **Clock Divider:** `CLOCK_DIV = 12000` slows the Pico's 48 MHz ADC clock to produce a ~4 kHz sample rate (48,000,000 / 12,000 = 4,000 samples/sec).

### Multicore Programming
- **Core 0 / Core 1:** The RP2350 has two independent CPU cores. Core 0 runs `main()` (audio + ML), Core 1 runs `core1_entry()` (LED state machine).
- **`multicore_launch_core1()`:** Starts execution on Core 1 from Core 0.
- **Hardware FIFO:** A 4-entry hardware buffer (`multicore_fifo_push_blocking()`, `multicore_fifo_pop_blocking()`, `multicore_fifo_rvalid()`) used to safely pass data between the two cores without locks or shared memory race conditions.

### GPIO & Timing
- **`gpio_init()` / `gpio_set_dir()` / `gpio_put()`:** Control the onboard LED on GP25.
- **`time_us_64()`:** High-resolution 64-bit microsecond timer, used for the 1-second cooldown between "on" keyword detections.
- **`gpio_disable_pulls()`:** Required on ADC pins for the RP2350-E9 silicon errata — prevents internal pull resistors from corrupting analog readings.

### Memory Management
- **Static buffers:** `capture_buf[1000]` (uint16), `intermediate_buf[4000]` (uint16), `features[4000]` (float) are all statically allocated global arrays — no heap allocation, no risk of fragmentation.
- **RAM constraint:** RP2350 has 520 KB of SRAM. Keeping `NSAMP` and `INSIZE` small is critical to avoid overflowing.

---

## 2. Machine Learning on Microcontrollers (TinyML)

### Edge Impulse Platform
- **Impulse:** The full processing pipeline — takes raw audio in, outputs classification labels.
- **DSP Block (MFCC/MFE):** Converts 1-second of raw audio into a spectrogram-like feature map. This step filters out noise and emphasizes the frequency patterns specific to human speech.
- **Neural Network:** A small Keras-based classifier trained on the MFCC features. Outputs a confidence score (0.0â€“1.0) for each label.
- **Deployment:** Edge Impulse exports the trained model as a self-contained C++ library with a single `run_classifier()` function — no Python runtime required on-device.

### Inference
- **`run_classifier()`:** The main entry point into the Edge Impulse SDK. Takes a `signal_t` struct (which points to the `features[]` float array) and writes results into `ei_impulse_result_t`.
- **`EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE`:** A constant defined by the exported model. It must equal your `INSIZE` (4000). A mismatch is a common build/runtime error.
- **`EI_CLASSIFIER_LABEL_COUNT`:** The number of labels in your model. Used to iterate over classification results.
- **`result.classification[ix].value`:** The confidence score for label at index `ix`. Labels are sorted alphabetically, so index order changes as you add new labels.

### Keyword Spotting
- **Sliding Window:** Rather than feeding the model fixed 1-second chunks, the firmware uses a sliding/overlapping window. Every 250ms, 1000 new samples are added and 1000 old samples are dropped. This means the model runs 4× per second and can catch a word that falls across chunk boundaries.
- **Threshold (`thresh = 0.7`):** A confidence score above this value is treated as a positive detection. Too low → false positives; too high → missed detections. Tune after initial testing.
- **Cooldown (`COOLDOWN_US = 1,000,000`):** Prevents the same keyword from being detected multiple times during a single utterance. After a "activate" fires, it is locked out for 1 second.

---

## 3. Audio Signal Processing

### Normalization
- The raw ADC output (`uint16_t`, range ~4095) varies in amplitude depending on microphone distance, volume, and ambient noise.
- Before feeding to the model, the firmware normalizes: `val = ((raw - min) / (max - min)) * 2 - 1`, then scales to `* 32766`. This stretches the audio dynamically to always fill the full range, making the model input consistent regardless of speaking volume.

### Sample Rate
- `CLOCK_DIV = 12000` → sample rate ≈ 4000 Hz.
- The Nyquist theorem states you can capture frequencies up to half the sample rate (2000 Hz). Human speech consonants (like "c" in "cancel") mostly live below 2000 Hz, so 4 kHz is sufficient for keyword spotting.
- The sample rate in firmware **must exactly match** the sample rate declared in Edge Impulse (4000 Hz) and used when writing WAV files (`scipy.io.wavfile.write(f, 4000, data)`).

### Base64 Encoding (pico-daq)
- The `pico-daq` firmware encodes raw float audio data as Base64 ASCII text before sending it over USB serial.
- This is necessary because USB serial (CDC) transmits printable text characters. Raw binary data (including floats) contains bytes that can be interpreted as control characters, causing corruption.
- The Python script `b64_float_to_wave.py` reverses this: it decodes the Base64 string back to floats and writes them as a WAV file at 4000 Hz.

---

## 4. WS2812 LED Strip Control

### NeoPixel Protocol
- WS2812 LEDs use a **single-wire protocol** where each LED's GRB color data is sent as a series of precisely timed pulses.
- **Timing is critical** — each bit must be transmitted with nanosecond-level accuracy. Standard GPIO toggling in software is not fast enough or reliable enough.

### PIO (Programmable I/O)
- The RP2350's **PIO state machines** are small, independent processors that run simple programs at the system clock frequency (up to 150 MHz). The Adafruit NeoPixel library uses a PIO program to generate the exact WS2812 timing in hardware, leaving the CPU completely free.
- The LED strip is connected to **GP7**. This pin is chosen because it can be assigned to a PIO state machine.

### LED State Machine
- `strip.begin()` — initializes the PIO program and allocates a state machine.
- `strip.setBrightness(64)` — global brightness scale (0â€“255). 64 = ~25% brightness.
- `strip.setPixelColor(i, strip.Color(R, G, B))` — sets a pixel's color in RAM.
- `strip.show()` — transmits all color data from RAM to the physical strip via PIO.
- `strip.clear()` — sets all pixels to off (black) in RAM (still need `strip.show()` to apply).

---

## 5. Build System

### CMake
- `CMakeLists.txt` defines the project, source files, compiler flags, and which Pico SDK libraries to link.
- `pico_sdk_import.cmake` locates the Pico SDK using the `PICO_SDK_PATH` environment variable.
- `set(PICO_BOARD pico2)` tells the SDK you are targeting the RP2350 chip (Pico 2), not the RP2040 (Pico 1).

### Key CMake Targets (pico-light-voice)
- `target_link_libraries(pico-voice ... hardware_adc hardware_dma pico_multicore ...)` — links the required Pico SDK hardware libraries.
- The Edge Impulse SDK is included as a set of source files and include directories, compiled as part of the project.

### Build Output
- `pico-voice.uf2` — the final firmware image. This is dragged onto the `RPI-RP2` USB mass-storage drive to flash the Pico.
- `.uf2` (USB Flashing Format) is a special binary format that the Pico's built-in bootloader understands. No external programmer or debug probe is needed.

---

## 6. Data Collection & Dataset Preparation

### pico-daq Firmware
- A separate firmware project that uses the same ADC/DMA setup as the main firmware, but streams audio data over USB serial instead of running ML.
- Captures 10,000 samples (~2.5 sec) per DMA burst, normalizes to float, encodes as Base64, and prints to serial.
- Double-buffering: captures the next chunk while transmitting the previous one to avoid gaps.

### Edge Impulse Data Upload
- WAV files must be at **4000 Hz** sample rate and approximately **1 second** in duration.
- Edge Impulse's "Split sample" tool can automatically slice a long recording into 1-second windows based on audio energy peaks.
- Labels must be consistent: `activate`, `cancel`, `noise`, `other`. Label names are case-sensitive and determine the alphabetical index order in the exported model.

### File Naming Convention
- Files in this project follow the `name.xx.wav` format (e.g., `ON.00.wav`, `noise.12.wav`).
- Zero-padded two-digit numbers ensure files sort correctly in any file manager or terminal.
