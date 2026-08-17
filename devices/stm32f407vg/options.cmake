set(DEVICE_FAMILY_DEFS 
  STM32F4
  STM32F407xx
)

set(DEVICE_CORE           cortex-m4)
set(DEVICE_FPU            auto)

set(DEVICE_CMSIS_INCLUDE  lib/cmsis_device_f4/Include)
set(DEVICE_STARTUP_SRC    lib/cmsis_device_f4/Source/Templates/gcc/startup_stm32f407xx.s)
set(DEVICE_SYSTEM_SRC     lib/cmsis_device_f4/Source/Templates/system_stm32f4xx.c)