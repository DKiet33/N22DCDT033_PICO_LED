# Voice-Activated LED Strip Controller (Raspberry Pi Pico 2)

An embedded C++ TinyML project that uses a **Raspberry Pi Pico 2 (RP2350)** and a microphone to control a WS2812/NeoPixel LED strip via voice commands. 

This system runs **fully offline** on the microcontroller—no Wi-Fi, no cloud processing, no companion smartphone app. It listens for the keywords **"activate"** and **"cancel"** using a 1D Convolutional Neural Network trained on Edge Impulse.

---

## 🌟 Features

* **100% Offline Keyword Spotting**: Local, continuous audio inference running on the RP2350.
* **Dual-Core Architecture**:
  * **Core 0**: Handles audio sampling (ADC + DMA) at 4 kHz and runs the Edge Impulse Machine Learning model.
  * **Core 1**: Dedicated to the LED State Machine and driving the NeoPixel strip via PIO (Programmable I/O) to ensure perfectly smooth animations without interrupting the ML inference.
* **14 Dynamic Lighting Modes**: Includes static colors, breathing, sequential wipes, fire/flame simulations, meteor rain, Cylon scanners, theater chases, sparkles, and dual-comets.
* **Inter-core Communication**: Uses the RP2350's hardware FIFO queue for thread-safe command passing between the ML engine and the LED controller.

---

## 🏗️ Project Architecture

```
[Your Voice] -> [MAX9814 Mic] -> [Pico 2 ADC] -> [DMA Buffer] -> [Edge Impulse CNN]
                                                                        |
                                                                        v (FIFO)
[WS2812 LED Strip] <- [PIO State Machine] <- [14-State Animation Logic] +
```

---

## 📁 Repository Structure

* **`awulff-pico-playground-main/`**: The core source code.
  * `pico-light-voice/`: The main C++ firmware for the voice-controlled LEDs.
    * `source/main.cpp`: Core 0 audio sampling and Edge Impulse inference loop.
    * `source/lights.cpp`: Core 1 LED state machine and animation effects.
  * `pico-daq/`: Data acquisition firmware used to record and stream raw audio over serial for training.
* **`dataset/`**: Contains the WAV files in two categories (`training` and `testing`) used to train the machine learning model.
* **`requirements.md`**: Detailed hardware, software, and configuration requirements.
* **`project_plan.md`**: Step-by-step guide to reproducing the project from scratch.
* **`phase.md`**: Project tracking and completion checklist.
* **`flow.md`**: Deep-dive into the data flow, FIFO protocols, and system states.
* **`skill.md`**: Educational reference detailing the Embedded C++, DSP, and TinyML concepts used.

---

## 🚀 Getting Started

If you want to build this project yourself, please refer to the comprehensive markdown guides included in this repository. 

**Recommended Reading Order:**
1. Read **`requirements.md`** to gather the necessary hardware (Pico 2, MAX9814, WS2812 strip, 5V supply).
2. Follow **`project_plan.md`** to set up your C++ toolchain and wire the components.
3. Use the `pico-daq` firmware to collect your own voice samples, or use the provided `dataset/`.
4. Train your model on Edge Impulse and export the C++ Library into the `pico-light-voice/` folder.
5. Build using CMake/NMake and flash the `.uf2` file to your Pico.

---

## 👏 Credits & Acknowledgements

This project was built upon the incredible open-source work of several individuals and organizations:

* **[Alex Wulff](https://github.com/AlexFWulff)**: The baseline architecture for capturing ADC audio via DMA and piping it into Edge Impulse was adapted from his **[awulff-pico-playground](https://github.com/AlexFWulff/awulff-pico-playground)** repository.
* **[Edge Impulse](https://www.edgeimpulse.com/)**: For providing the TinyML platform used to train, quantize (int8), and export the 1D-CNN keyword spotting model as a highly optimized C++ library.
* **[Raspberry Pi Foundation](https://github.com/raspberrypi/pico-sdk)**: For the fantastic `pico-sdk` which makes multicore processing, DMA, and PIO state machines highly accessible.
* **[Adafruit Industries](https://github.com/adafruit/Adafruit_NeoPixel)**: For the foundational NeoPixel protocol logic and standard libraries.
* **[Martin Kooij](https://github.com/martinkooij/pi-pico-adafruit-neopixels)**: For the excellent `pi-pico-adafruit-neopixels` port which adapted Adafruit's libraries to use the Raspberry Pi Pico PIO state machines seamlessly.
