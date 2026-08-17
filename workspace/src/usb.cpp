#include "stepper_synth/usb.hpp"

#include <assert.h>

#include "stm32f407xx.h"

void usbInit() {
  // USB Pin AF Configuration (PA11, PA12)
  GPIOA->MODER |= 0b10u << GPIO_MODER_MODE11_Pos;  // Set AF Mode
  GPIOA->MODER |= 0b10u << GPIO_MODER_MODE12_Pos;  //
  GPIOA->AFR[1] |= 10u << GPIO_AFRH_AFSEL11_Pos;   // Select AF10 (OTG)
  GPIOA->AFR[1] |= 10u << GPIO_AFRH_AFSEL12_Pos;   //

  GPIOA->OSPEEDR |= (0b11u << GPIO_OSPEEDR_OSPEED11_Pos) |  // Set high speed
                    (0b11u << GPIO_OSPEEDR_OSPEED12_Pos);   //

  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD11 | GPIO_PUPDR_PUPD12);  // Disable pulls

  // Disable VBUS (IMPORTANT)
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
  USB_OTG_FS->GCCFG &= ~(USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_VBUSASEN);

  assert(tusb_init());
  assert(tud_init(0));
}

extern "C" void OTG_FS_IRQHandler(void) { tud_int_handler(0); }