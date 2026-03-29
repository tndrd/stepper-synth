set(DEVICE_FAMILY_DEFS 
  STM32G4
  STM32G431xx
)

set(DEVICE_CORE           cortex-m4)
set(DEVICE_FPU            auto)

set(DEVICE_CMSIS_INCLUDE  lib/cmsis-device-g4/Include)
set(DEVICE_STARTUP_SRC    lib/cmsis-device-g4/Source/Templates/gcc/startup_stm32g431xx.s)
set(DEVICE_SYSTEM_SRC     lib/cmsis-device-g4/Source/Templates/system_stm32g4xx.c)