// clang-format off
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
#include "stm32f407xx.h"

#ifdef __cplusplus
extern "C" uint32_t SystemCoreClock;
extern "C" void vAssertCalled(const char*, int);
#else
extern uint32_t SystemCoreClock;
extern void vAssertCalled(const char*, int);
#endif

#define configCPU_CLOCK_HZ       ( SystemCoreClock )
#define configTICK_RATE_HZ       1000
#define configMAX_PRIORITIES     5
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE    ( 16 * 1024 )

#define configUSE_16_BIT_TICKS 0
#define configUSE_IDLE_HOOK    0
#define configUSE_TICK_HOOK    0

#define configPRIO_BITS __NVIC_PRIO_BITS

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY			  ( (1 << configPRIO_BITS) - 1 )
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY	3

#define configKERNEL_INTERRUPT_PRIORITY \
  ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
	
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler
#define xPortSysTickHandler  SysTick_Handler

#define configUSE_PREEMPTION   1
#define configUSE_TIME_SLICING 0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1

#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION  0

#define configMAX_TASK_NAME_LEN 32

#define configASSERT(x) \
  do {if ((x) == 0) vAssertCalled(__FILE__, __LINE__);} while(0)

#define configUSE_MALLOC_FAILED_HOOK   1
#define configCHECK_FOR_STACK_OVERFLOW 2

#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 0
#define configTIMER_TASK_STACK_DEPTH 128
#define configTIMER_QUEUE_LENGTH 4

#define configUSE_MUTEXES 1

#define INCLUDE_vTaskDelay 1

#endif
// clang-format on