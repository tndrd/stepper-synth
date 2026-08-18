#include "FreeRTOS.h"
#include "task.h"
#include <assert.h>

void vAssertCalled(const char* file, int line) {
    __assert_func(file, line, __ASSERT_FUNC, "");
}

void vApplicationMallocFailedHook(void) {
    __assert_func("", 0, __ASSERT_FUNC, "");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name) {
  __assert_func("", 0, __ASSERT_FUNC, name);
}