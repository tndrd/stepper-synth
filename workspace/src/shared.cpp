#include "stepper_synth/shared.hpp"

PWMGenerator* volatile s_pwm_gen_ptr = nullptr;
volatile uint32_t g_ms_ticks = 0;