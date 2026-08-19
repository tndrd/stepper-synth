cmake_minimum_required(VERSION 3.20)
project(stepper_synth C CXX ASM)

# Configure and add FreeRTOS target
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE
  ${CMAKE_SOURCE_DIR}/workspace/freertos/
)
set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "" FORCE)
set(FREERTOS_HEAP 4 CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/FreeRTOS-Kernel)

# Add hooks to kernel sources
target_sources(freertos_kernel PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/freertos/hooks.c
)

# Add tinyUSB target
set(TUSB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/tinyusb)
add_library(tusb
  ${TUSB_DIR}/src/tusb.c
  ${TUSB_DIR}/src/common/tusb_fifo.c
  ${TUSB_DIR}/src/class/midi/midi_device.c
  ${TUSB_DIR}/src/device/usbd.c
  ${TUSB_DIR}/src/device/usbd_control.c
  ${TUSB_DIR}/src/portable/synopsys/dwc2/dcd_dwc2.c
  ${TUSB_DIR}/src/portable/synopsys/dwc2/dwc2_common.c

  ${CMAKE_CURRENT_SOURCE_DIR}/tusb/usb_descriptors.c
)
target_include_directories(tusb PUBLIC
  ${TUSB_DIR}/src/
  ${CMAKE_CURRENT_SOURCE_DIR}/tusb/
)
target_link_libraries(tusb PUBLIC freertos_kernel)

# Add application target
# OBJECT because otherwise linker may ignore
# weak isr redefinitions
add_library(synth_main OBJECT
  ${CMAKE_CURRENT_SOURCE_DIR}/src/12tet.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/board.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/midi.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/note_scheduler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pwm_generator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/usb.cpp
)
target_include_directories(synth_main PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/
)
target_link_libraries(synth_main PUBLIC tusb freertos_kernel)
