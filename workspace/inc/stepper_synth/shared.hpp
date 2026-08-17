#pragma once
#include "pwm_generator.hpp"

extern PWMGenerator* volatile s_pwm_gen_ptr;
extern volatile uint32_t g_ms_ticks;