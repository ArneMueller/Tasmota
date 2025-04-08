/*
  xsns_128_vk16k.ino - Interface to led outputs of 8x8 led driver chips like VK16K33C for Tasmota

  Copyright (C) 2025  Arne Reimers

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_VK16K
/*********************************************************************************************\
 * VK16K33C led driver interfaced on the led outputs instead of i2c
\*********************************************************************************************/

#define XSNS_128         128

#ifndef VK16K_MAX_GRID
#define VK16K_MAX_GRID   3
#endif
#ifndef VK16K_MAX_SEG
#define VK16K_MAX_SEG    4
#endif

struct VK16K {
  int8_t grid_pin[VK16K_MAX_GRID] = {-1};
  int8_t seg_pin[VK16K_MAX_SEG] = {-1};
  uint8_t grid[VK16K_MAX_GRID] = {0};
  uint8_t tmp_grid[VK16K_MAX_GRID] = {0};
  bool detected = false;
} Vk16k;

/*********************************************************************************************\
 * 
\*********************************************************************************************/

void IRAM_ATTR Vk16kGridRead(void *arg);
void Vk16kGridRead(void *arg) {
  uint32_t g = (uint32_t) arg;
  if (digitalRead(Vk16k.grid_pin[g]) != 0) {
    return;
  }
  for (uint32_t s = 0; s < VK16K_MAX_SEG; s++) {
    if(Vk16k.seg_pin[s]>=0) {
      Vk16k.tmp_grid[g] |= digitalRead(Vk16k.seg_pin[s]) << s;
    }
  }
}

void IRAM_ATTR Vk16kSegRead(void *arg);
void Vk16kSegRead(void *arg) {
  uint32_t s = (uint32_t) arg;
  if (digitalRead(Vk16k.seg_pin[s]) != 1) {
    return;
  }
  for (uint32_t g = 0; g < VK16K_MAX_GRID; g++) {
    if(Vk16k.grid_pin[g]>=0) {
      Vk16k.tmp_grid[g] |= (1-digitalRead(Vk16k.grid_pin[g])) << s;
    }
  }
}


/*********************************************************************************************/

void VkInit(void) {
  for (uint8_t g = 0; g < VK16K_MAX_GRID; g++) {
    if (PinUsed(GPIO_VK16K_GRID, g)) {
      Vk16k.grid_pin[g] = Pin(GPIO_VK16K_GRID, g);
      // Pull-up on
      pinMode(Vk16k.grid_pin[g], INPUT_PULLUP);
      attachInterruptArg(Vk16k.grid_pin[g], Vk16kGridRead, (void*)g, FALLING);
      Vk16k.detected = true;
    }
  }
  for (uint8_t s = 0; s < VK16K_MAX_SEG; s++) {
    if (PinUsed(GPIO_VK16K_SEG, s)) {
      Vk16k.seg_pin[s] = Pin(GPIO_VK16K_SEG, s);
      // Pull-up off
      pinMode(Vk16k.seg_pin[s], INPUT);
      attachInterruptArg(Vk16k.seg_pin[s], Vk16kSegRead, (void*)s, RISING);
      Vk16k.detected = true;
    }
  }
}

void VkInterruptDisable(void) {
  for (uint8_t g = 0; g < VK16K_MAX_GRID; g++) {
    if (Vk16k.grid_pin[g] >= 0) {
      detachInterrupt(Vk16k.grid_pin[g]);
    }
  }
  for (uint8_t s = 0; s < VK16K_MAX_SEG; s++) {
    if (Vk16k.seg_pin[s] >= 0) {
      detachInterrupt(Vk16k.seg_pin[s]);
    }
  }
}

void VkLoop(void) {
  for (uint32_t g = 0; g < VK16K_MAX_GRID; g++) {
    if(Vk16k.grid_pin[g] >= 0) {
      Vk16k.grid[g] = Vk16k.tmp_grid[g];
      Vk16k.tmp_grid[g] = 0;
    }
  }
}

void VkJson(void) {
  ResponseAppend_P(PSTR(",\"Vk16k\":{"));
  for(uint32_t g = 0; g < VK16K_MAX_GRID; g++) {
    if(Vk16k.grid_pin[g] >= 0) {
      ResponseAppend_P(PSTR("%s\"%i\":%i"),
          g==0?"":",",
          g,
          Vk16k.grid[g]);
    }
  }
  ResponseJsonEnd();
  return;
}

#ifdef USE_WEBSERVER
const char HTTP_VK16K_GRID[] PROGMEM = "{s}grid%i{m}%8_b{e}";
void VkWeb(void) {
  WSContentSend_P(HTTP_SNS_HR_THIN);
  for(uint32_t g = 0; g < VK16K_MAX_GRID; g++) {
    if(Vk16k.grid_pin[g] >= 0) {
      WSContentSend_P(HTTP_VK16K_GRID,g,
          Vk16k.grid[g]);
      }
  }
  return;
}
#endif  // USE_WEBSERVER


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns128(uint32_t function) {
  bool result = false;

  if (FUNC_SETUP_RING2 == function) {
    VkInit();
  } else if (Vk16k.detected) {
    switch (function) {
      case FUNC_EVERY_50_MSECOND:
        VkLoop();
        break;
      case FUNC_JSON_APPEND:
        VkJson();
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        VkWeb();
        break;
#endif  // USE_WEBSERVER
     case FUNC_INTERRUPT_STOP:
        VkInterruptDisable();
        break;
      case FUNC_INTERRUPT_START:
        VkInit();
        break;
     case FUNC_ACTIVE:
        result = true;
        break;
    }
  }
  return result;
}

#endif  // USE_VK16K
