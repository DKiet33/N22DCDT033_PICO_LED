#include "Adafruit_NeoPixel.hpp"
#include <pico/multicore.h>
#include <stdio.h>
#include <stdlib.h>

#define PIN 7
#define NUM_STATES 14
#define NUM_LIGHTS 24

bool update_state(uint32_t *state, bool strip_on) {
  if (multicore_fifo_rvalid()) {
    uint32_t val = multicore_fifo_pop_blocking();
    if (val == 2) {
      printf("Turning lights off (cancel)\n");
      multicore_fifo_push_blocking(0);
      return false;
    } else {
      if (!strip_on) {
        printf("Turning lights on (activate)\n");
        multicore_fifo_push_blocking(0);
        return true;
      }
      *state = (*state + 1) % NUM_STATES;
      printf("Incrementing state to %u\n", *state);
      multicore_fifo_push_blocking(0);
      return true;
    }
  }
  return strip_on;
}

// Helper for rainbow colors
uint32_t Wheel(uint8_t WheelPos, Adafruit_NeoPixel &strip) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// Helper for fading to black
void fadeToBlack(int ledNo, uint8_t fadeValue, Adafruit_NeoPixel &strip) {
  uint32_t oldColor = strip.getPixelColor(ledNo);
  uint8_t r = (oldColor >> 16) & 0xff;
  uint8_t g = (oldColor >> 8) & 0xff;
  uint8_t b = oldColor & 0xff;

  r = (r <= 10) ? 0 : (int)r - (r * fadeValue / 256);
  g = (g <= 10) ? 0 : (int)g - (g * fadeValue / 256);
  b = (b <= 10) ? 0 : (int)b - (b * fadeValue / 256);

  strip.setPixelColor(ledNo, r, g, b);
}

// Helper for fire heat color
void setPixelHeatColor(int Pixel, uint8_t temperature,
                       Adafruit_NeoPixel &strip) {
  // Scale 'heat' down from 0-255 to 0-191
  uint8_t t192 = (temperature / 255.0) * 191;
  uint8_t heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2;                 // scale up to 0..252
  if (t192 > 0x80) {
    strip.setPixelColor(Pixel, 255, 255, heatramp);
  } else if (t192 > 0x40) {
    strip.setPixelColor(Pixel, 255, heatramp, 0);
  } else {
    strip.setPixelColor(Pixel, heatramp, 0, 0);
  }
}

void core1_entry() {
  Adafruit_NeoPixel strip =
      Adafruit_NeoPixel(NUM_LIGHTS, PIN, NEO_GRB + NEO_KHZ800);
  strip.begin();
  strip.setBrightness(64);

  multicore_fifo_push_blocking(0);

  uint32_t state = 0;
  bool lights_on = false;

  uint32_t rainbow_colors[7] = {
      strip.Color(255, 0, 0),   // Red
      strip.Color(255, 127, 0), // Orange
      strip.Color(255, 255, 0), // Yellow
      strip.Color(0, 255, 0),   // Green
      strip.Color(0, 0, 255),   // Blue
      strip.Color(75, 0, 130),  // Indigo
      strip.Color(148, 0, 211)  // Violet
  };

  // State 6 variables
  bool random_color_initialized = false;
  uint32_t current_random_color = 0;

  // State 7 (Fire) array
  uint8_t heat[NUM_LIGHTS];
  for (int i = 0; i < NUM_LIGHTS; i++)
    heat[i] = 0;

  // Track previous state to detect transitions
  uint32_t prev_state = 0;

  srand(time_us_32());

  while (1) {
    lights_on = update_state(&state, lights_on);

    // Clear strip on state transition
    if (state != prev_state) {
      strip.clear();
      strip.show();
      prev_state = state;
    }

    if (!lights_on) {
      strip.clear();
      strip.show();
      sleep_ms(200);
      continue;
    }

    // State 0: Rainbow Flow Forward
    if (state == 0) {
      for (uint16_t j = 0; j < 256; j++) {
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
          strip.setPixelColor(
              i, Wheel(((i * 256 / strip.numPixels()) + j) & 255, strip));
        }
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
        strip.show();
        sleep_ms(10);
      }
    }
    // State 1: Rainbow Flow Reverse
    else if (state == 1) {
      for (int j = 255; j >= 0; j--) {
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
          strip.setPixelColor(
              i, Wheel(((i * 256 / strip.numPixels()) + j) & 255, strip));
        }
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
        strip.show();
        sleep_ms(10);
      }
    }
    // State 2: Breathing 7 colors
    else if (state == 2) {
      for (int c_idx = 0; c_idx < 7; c_idx++) {
        for (int b = 0; b <= 64; b += 2) {
          strip.setBrightness(b);
          for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, rainbow_colors[c_idx]);
          }
          strip.show();
          sleep_ms(20);
          uint32_t last_state = state;
          lights_on = update_state(&state, lights_on);
          if (last_state != state || !lights_on)
            break;
        }
        if (state != 2 || !lights_on)
          break;

        for (int b = 64; b >= 0; b -= 2) {
          strip.setBrightness(b);
          for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, rainbow_colors[c_idx]);
          }
          strip.show();
          sleep_ms(20);
          uint32_t last_state = state;
          lights_on = update_state(&state, lights_on);
          if (last_state != state || !lights_on)
            break;
        }
        if (state != 2 || !lights_on)
          break;
      }
      strip.setBrightness(64); // Reset brightness for other states
    }
    // State 3: Flashing 7 colors
    else if (state == 3) {
      for (int c_idx = 0; c_idx < 7; c_idx++) {
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
          strip.setPixelColor(i, rainbow_colors[c_idx]);
        }
        strip.show();
        sleep_ms(150);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;

        strip.clear();
        strip.show();
        sleep_ms(150);
        last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
    }
    // State 4: Rainbow Wipe Forward
    else if (state == 4) {
      strip.clear();
      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i,
                            Wheel((i * 256 / strip.numPixels()) & 255, strip));
        strip.show();
        sleep_ms(50);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
      if (state != 4 || !lights_on)
        continue;

      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, 0);
        strip.show();
        sleep_ms(50);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
    }
    // State 5: Rainbow Wipe Reverse
    else if (state == 5) {
      strip.clear();
      for (int i = strip.numPixels() - 1; i >= 0; i--) {
        strip.setPixelColor(i,
                            Wheel((i * 256 / strip.numPixels()) & 255, strip));
        strip.show();
        sleep_ms(50);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
      if (state != 5 || !lights_on)
        continue;

      for (int i = strip.numPixels() - 1; i >= 0; i--) {
        strip.setPixelColor(i, 0);
        strip.show();
        sleep_ms(50);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
    }
    // State 6: Static Random Color
    else if (state == 6) {
      if (!random_color_initialized) {
        current_random_color =
            strip.Color(rand() % 256, rand() % 256, rand() % 256);
        random_color_initialized = true;
      }
      for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, current_random_color);
      }
      strip.show();
      sleep_ms(100);
      uint32_t last_state = state;
      lights_on = update_state(&state, lights_on);
      if (last_state != state) {
        random_color_initialized = false; // reset for next time
      }
    }
    // State 7: Fire / Flame
    else if (state == 7) {
      int cooling = 55;
      int sparking = 120;
      for (int i = 0; i < strip.numPixels(); i++) {
        int cooldown = rand() % (((cooling * 10) / strip.numPixels()) + 2);
        if (cooldown > heat[i]) {
          heat[i] = 0;
        } else {
          heat[i] = heat[i] - cooldown;
        }
      }
      for (int k = strip.numPixels() - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
      }
      if (rand() % 255 < sparking) {
        int y = rand() % 7;
        if (y < strip.numPixels()) {
          int newHeat = (int)heat[y] + 160 + rand() % 95;
          heat[y] = (newHeat > 255) ? 255 : (uint8_t)newHeat;
        }
      }
      for (int j = 0; j < strip.numPixels(); j++) {
        setPixelHeatColor(j, heat[j], strip);
      }
      strip.show();
      sleep_ms(30);
      uint32_t last_state = state;
      lights_on = update_state(&state, lights_on);
      if (last_state != state || !lights_on) {
        for (int i = 0; i < NUM_LIGHTS; i++)
          heat[i] = 0;
      }
    }
    // State 8: Meteor Rain
    else if (state == 8) {
      uint8_t meteorSize = 5;
      uint8_t meteorTrailDecay = 64;
      uint32_t meteorColor = strip.Color(0, 255, 255); // Cyan meteor

      while (state == 8 && lights_on) {
        for (int i = 0; i < strip.numPixels() + strip.numPixels(); i++) {
          // 1. Fade brightness all LEDs to create the trail
          for (int j = 0; j < strip.numPixels(); j++) {
            fadeToBlack(j, meteorTrailDecay, strip);
          }
          // 2. Draw meteor head
          for (int j = 0; j < meteorSize; j++) {
            if ((i - j < strip.numPixels()) && (i - j >= 0)) {
              strip.setPixelColor(i - j, meteorColor);
            }
          }
          strip.show();
          sleep_ms(30);

          uint32_t last_state = state;
          lights_on = update_state(&state, lights_on);
          if (last_state != state || !lights_on)
            break;
        }
      }
    }
    // State 9: Cylon / Larson Scanner
    else if (state == 9) {
      uint32_t color = strip.Color(255, 0, 0); // Red
      uint8_t EyeSize = 2;
      uint8_t returnDelay = 40;

      for (int i = 0; i < strip.numPixels() - EyeSize - 2; i++) {
        strip.clear();
        strip.setPixelColor(i, (color & 0xFEFEFE) >> 1); // 50% brightness
        for (int j = 1; j <= EyeSize; j++) {
          strip.setPixelColor(i + j, color);
        }
        strip.setPixelColor(i + EyeSize + 1,
                            (color & 0xFEFEFE) >> 1); // 50% brightness
        strip.show();
        sleep_ms(returnDelay);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
      if (state != 9 || !lights_on)
        continue;

      for (int i = strip.numPixels() - EyeSize - 2; i >= 0; i--) {
        strip.clear();
        strip.setPixelColor(i, (color & 0xFEFEFE) >> 1); // 50% brightness
        for (int j = 1; j <= EyeSize; j++) {
          strip.setPixelColor(i + j, color);
        }
        strip.setPixelColor(i + EyeSize + 1,
                            (color & 0xFEFEFE) >> 1); // 50% brightness
        strip.show();
        sleep_ms(returnDelay);
        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
    }
    // State 10: Theater Chase
    else if (state == 10) {
      uint32_t color = strip.Color(255, 255, 0); // Yellow
      for (int j = 0; j < 10; j++) {             // do 10 cycles of chasing
        for (int q = 0; q < 3; q++) {
          for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
            if (i + q < strip.numPixels())
              strip.setPixelColor(i + q, color); // turn every third pixel on
          }
          strip.show();
          sleep_ms(50);
          uint32_t last_state = state;
          lights_on = update_state(&state, lights_on);
          if (last_state != state || !lights_on)
            break;

          for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
            if (i + q < strip.numPixels())
              strip.setPixelColor(i + q, 0); // turn every third pixel off
          }
        }
        if (state != 10 || !lights_on)
          break;
      }
    }
    // State 11: Sparkle / Strobe
    else if (state == 11) {
      while (state == 11 && lights_on) {
        strip.clear();
        // Pick 2-4 random pixels to sparkle this frame
        int count = (rand() % 3) + 2;
        for (int i = 0; i < count; i++) {
          int pixel = rand() % strip.numPixels();
          strip.setPixelColor(pixel, 255, 255, 255); // White sparkle
        }
        strip.show();
        sleep_ms(20);

        // Quick fade out/off
        strip.clear();
        strip.show();
        sleep_ms(rand() % 50 + 20); // Fast, energetic timing

        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on)
          break;
      }
    }
    // State 12: Color Twinkle
    else if (state == 12) {
      int Pixel = rand() % strip.numPixels();
      uint32_t color = strip.Color(rand() % 256, rand() % 256, rand() % 256);
      strip.setPixelColor(Pixel, color);
      strip.show();
      sleep_ms(100);
      // Let it linger, then randomly fade things out slowly
      for (int j = 0; j < strip.numPixels(); j++) {
        if (rand() % 10 > 5) {
          fadeToBlack(j, 20, strip);
        }
      }
      uint32_t last_state = state;
      lights_on = update_state(&state, lights_on);
      if (last_state != state || !lights_on) {
        strip.clear();
        strip.show();
      }
    }
    // State 13: Comets
    else if (state == 13) {
      static uint32_t cometColor1, cometColor2;
      static uint32_t last_comet_state = 999;

      // Pick new random colors only when entering the state
      if (last_comet_state != state) {
        cometColor1 = strip.Color(rand() % 256, rand() % 256, rand() % 256);
        cometColor2 = strip.Color(rand() % 256, rand() % 256, rand() % 256);
        last_comet_state = state;
      }

      while (state == 13 && lights_on) {
        strip.clear();
        uint8_t cometSize = 4;

        // Comet 1
        int pos1 = (time_us_32() / 15000) % (strip.numPixels() + cometSize);
        // Comet 2 (Different speed)
        int pos2 = (time_us_32() / 20000) % (strip.numPixels() + cometSize);

        for (int k = 0; k < cometSize; k++) {
          // Draw Comet 1
          if ((pos1 - k >= 0) && (pos1 - k < strip.numPixels())) {
            float b1 = 1.0f - ((float)k / cometSize);
            uint8_t r1 = (cometColor1 >> 16) & 0xFF;
            uint8_t g1 = (cometColor1 >> 8) & 0xFF;
            uint8_t b1_val = cometColor1 & 0xFF;
            strip.setPixelColor(pos1 - k, (uint8_t)(r1 * b1),
                                (uint8_t)(g1 * b1), (uint8_t)(b1_val * b1));
          }
          // Draw Comet 2
          if ((pos2 - k >= 0) && (pos2 - k < strip.numPixels())) {
            float b2 = 1.0f - ((float)k / cometSize);
            uint8_t r2 = (cometColor2 >> 16) & 0xFF;
            uint8_t g2 = (cometColor2 >> 8) & 0xFF;
            uint8_t b2_val = cometColor2 & 0xFF;
            strip.setPixelColor(pos2 - k, (uint8_t)(r2 * b2),
                                (uint8_t)(g2 * b2), (uint8_t)(b2_val * b2));
          }
        }

        strip.show();
        sleep_ms(30);

        uint32_t last_state = state;
        lights_on = update_state(&state, lights_on);
        if (last_state != state || !lights_on) {
          last_comet_state = 999;
          strip.clear();
          strip.show();
          break;
        }
      }
    }
  }
}