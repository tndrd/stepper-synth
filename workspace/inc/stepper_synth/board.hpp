#pragma once

#include <stdint.h>

struct Clocks {
  uint32_t sys;
  uint32_t ahb1;
  uint32_t ahb2;
  uint32_t apb1;
  uint32_t apb1tim;
  uint32_t apb2;
  uint32_t apb2tim;
};

Clocks boardInit();
void boardEnableIRQ();
void boardSystickStart();