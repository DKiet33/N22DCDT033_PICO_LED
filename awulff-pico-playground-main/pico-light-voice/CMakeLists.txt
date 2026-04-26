cmake_minimum_required(VERSION 3.12)

# initialize pico-sdk from submodule
include(pico_sdk_import.cmake)

project(pico-light-voice C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

pico_sdk_init()

# Define the main executable target
add_executable(pico-voice
    source/main.cpp
    source/lights.cpp
)

# --- NeoPixel PIO Library ---
add_subdirectory(pico_neopixels)

# --- Edge Impulse SDK Static Library ---
# Glob all Edge Impulse sources
file(GLOB_RECURSE EI_SDK_SOURCES
    "edge-impulse-sdk/*.cpp"
    "edge-impulse-sdk/*.c"
    "edge-impulse-sdk/*.cc"
    "tflite-model/*.cpp"
    "tflite-model/*.c"
    "tflite-model/*.cc"
)

# Exclude unused porting targets to prevent compilation errors
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/arduino/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/espressif/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/himax/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/mbed/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/mingw32/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/posix/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/silabs/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/sony/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/stm32-cubeai/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/ti/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/zephyr/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/ethos-u/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/core-v/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/nordic/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/renesas/.*")
list(FILTER EI_SDK_SOURCES EXCLUDE REGEX "edge-impulse-sdk/porting/syntiant/.*")

add_library(ei_sdk STATIC ${EI_SDK_SOURCES})

# Include all the Edge Impulse directories
target_include_directories(ei_sdk PUBLIC
    .
    model-parameters
    tflite-model
    edge-impulse-sdk
    edge-impulse-sdk/classifier
    edge-impulse-sdk/porting
    edge-impulse-sdk/dsp
    edge-impulse-sdk/dsp/kissfft
    edge-impulse-sdk/dsp/dct
    edge-impulse-sdk/third_party/flatbuffers/include
    edge-impulse-sdk/third_party/gemmlowp
    edge-impulse-sdk/third_party/ruy
    edge-impulse-sdk/CMSIS/DSP/Include/
    edge-impulse-sdk/CMSIS/Core/Include/
    edge-impulse-sdk/tensorflow/
    edge-impulse-sdk/tensorflow/lite
    edge-impulse-sdk/tensorflow/lite/micro
    edge-impulse-sdk/tensorflow/lite/micro/kernels
    edge-impulse-sdk/tensorflow/lite/schema
    edge-impulse-sdk/tensorflow/lite/c
    edge-impulse-sdk/tensorflow/lite/core/api
)

# CRITICAL: Link pico_stdlib to ei_sdk so PICO_BOARD macros are passed down
# This fixes the ei_malloc / ei_printf undefined references
target_link_libraries(ei_sdk pico_stdlib)

# Disable certain warnings for Edge Impulse SDK
target_compile_options(ei_sdk PRIVATE -Wno-strict-aliasing -Wno-ignored-qualifiers)

# --- Link everything together ---
target_include_directories(pico-voice PRIVATE
    source
)

target_link_libraries(pico-voice
    pico_stdlib
    hardware_adc
    hardware_dma
    hardware_gpio
    pico_multicore
    pico_neopixel
    ei_sdk
)

# Enable USB output, disable UART output (if using USB for serial)
pico_enable_stdio_usb(pico-voice 1)
pico_enable_stdio_uart(pico-voice 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(pico-voice)
