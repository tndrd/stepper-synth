#include "stepper_synth/shared.hpp"

extern "C" void TIM2_IRQHandler(void) { s_pwm_gen_ptr->isr(); }
extern "C" void SysTick_Handler(void) { g_ms_ticks++; }
