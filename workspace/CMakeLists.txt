cmake_minimum_required(VERSION 3.20)
project(stepper_synth C CXX ASM)

add_library(tusb
  lib/tinyusb/src/tusb.c
  lib/tinyusb/src/common/tusb_fifo.c
  lib/tinyusb/src/class/midi/midi_device.c
  lib/tinyusb/src/device/usbd.c
  lib/tinyusb/src/device/usbd_control.c
  lib/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c
  lib/tinyusb/src/portable/synopsys/dwc2/dwc2_common.c

  usb_descriptors.c
)
target_include_directories(tusb PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/tinyusb/src
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(synth_main main.cpp synth.cpp common.cpp)
target_include_directories(synth_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(synth_main PUBLIC tusb)
