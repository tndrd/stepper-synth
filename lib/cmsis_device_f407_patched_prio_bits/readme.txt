This is a patched version of original cmsis_device_f4 package.
It includes patched versions of STM32F407-related files, and the only difference
from original is a different number of NVIC priority bits. It seems like STM32F4 on my particular
board is a knock-off and has three actual NVIC priority bits instead of four, which results in assertion
failures inside FreeRTOS. Number of actual priority bits may be determined with a simple test:

uint8_t old_ip = NVIC->IP[0];
NVIC->IP[0] = 0xff;
uint8_t actual_bits = NVIC->IP[0];
assert(actual_bits == 0xf0) // for f4
NVIC->IP[0] = old_ip;