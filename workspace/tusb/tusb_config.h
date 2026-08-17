// Adapted from tinyusb/examples/midi_test
// clang-format off

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE OPT_MODE_NONE
#define CFG_TUSB_MCU          OPT_MCU_STM32F4
#define CFG_TUSB_OS           OPT_OS_NONE
#define CFG_TUSB_DEBUG        0
#define CFG_TUSB_DEBUG_PRINTF dbg_showstr

#define CFG_TUD_MAX_SPEED      OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64 // because FS
#define CFG_TUD_ENABLED        1

#define CFG_TUD_MIDI 1
#define CFG_TUD_VENDOR 0

// MIDI FIFO size of TX and RX
#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64

#include <stdio.h>

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */