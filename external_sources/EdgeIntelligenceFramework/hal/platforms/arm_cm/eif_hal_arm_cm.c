/**
 * @file eif_hal_arm_cm.c
 * @brief ARM Cortex-M HAL Implementation
 * 
 * Supports Cortex-M0/M0+/M3/M4/M7
 * Requires CMSIS headers
 */

#include "eif_hal.h"

#if defined(EIF_HAL_PLATFORM_ARM)

#include <stdarg.h>
#include <stdio.h>

// CMSIS Core includes (if available)
#if __has_include("core_cm4.h")
    #include "core_cm4.h"
    #define HAS_CMSIS 1
#elif __has_include("core_cm7.h")
    #include "core_cm7.h"
    #define HAS_CMSIS 1
#elif __has_include("core_cm3.h")
    #include "core_cm3.h"
    #define HAS_CMSIS 1
#elif __has_include("core_cm0.h")
    #include "core_cm0.h"
    #define HAS_CMSIS 1
#else
    #define HAS_CMSIS 0
#endif

// Default system clock (can be overridden)
#ifndef SystemCoreClock
static uint32_t SystemCoreClock = 72000000;  // 72 MHz default
#endif

static uint32_t tick_count = 0;
static volatile uint32_t delay_counter = 0;

// =============================================================================
// SysTick Handler (if not provided by HAL)
// =============================================================================

#if HAS_CMSIS
__attribute__((weak)) void SysTick_Handler(void) {
    tick_count++;
    if (delay_counter > 0) {
        delay_counter--;
    }
}
#endif

// =============================================================================
// Timing
// =============================================================================

int eif_hal_timer_init(void) {
#if HAS_CMSIS
    // Configure SysTick for 1ms tick
    SysTick_Config(SystemCoreClock / 1000);
    return 0;
#else
    return -1;
#endif
}

uint64_t eif_hal_get_time_us(void) {
#if HAS_CMSIS
    uint32_t ms = tick_count;
    uint32_t ticks = SysTick->VAL;
    uint32_t reload = SysTick->LOAD;
    uint32_t us_in_tick = ((reload - ticks) * 1000) / reload;
    return (uint64_t)ms * 1000 + us_in_tick;
#else
    return 0;
#endif
}

uint32_t eif_hal_get_time_ms(void) {
    return tick_count;
}

void eif_hal_delay_ms(uint32_t ms) {
    delay_counter = ms;
    while (delay_counter > 0) {
        __WFI();  // Wait for interrupt (saves power)
    }
}

void eif_hal_delay_us(uint32_t us) {
    // Busy loop for microseconds
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    volatile uint32_t i;
    for (i = 0; i < cycles / 3; i++) {
        __NOP();
    }
}

// =============================================================================
// Memory
// =============================================================================

extern uint32_t _estack;  // End of stack (from linker)
extern uint32_t _Min_Heap_Size;

size_t eif_hal_heap_free(void) {
    // Rough estimate using stack pointer
    register uint32_t sp asm("sp");
    extern char end;  // End of BSS from linker
    return sp - (uint32_t)&end;
}

size_t eif_hal_heap_total(void) {
    return (size_t)&_Min_Heap_Size;
}

void* eif_hal_aligned_alloc(size_t size, size_t alignment) {
    // Simple aligned allocation using extra space
    void* raw = malloc(size + alignment);
    if (!raw) return NULL;
    
    uintptr_t addr = (uintptr_t)raw;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    
    // Store original pointer just before aligned block
    ((void**)aligned)[-1] = raw;
    
    return (void*)aligned;
}

void eif_hal_aligned_free(void* ptr) {
    if (ptr) {
        void* raw = ((void**)ptr)[-1];
        free(raw);
    }
}

// =============================================================================
// Critical Sections
// =============================================================================

uint32_t eif_hal_critical_enter(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void eif_hal_critical_exit(uint32_t state) {
    if ((state & 1) == 0) {
        __enable_irq();
    }
}

// =============================================================================
// Debug (ITM if available, otherwise SWO)
// =============================================================================

void eif_hal_debug_print(const char* fmt, ...) {
#if HAS_CMSIS && defined(ITM)
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    for (int i = 0; i < len && i < sizeof(buffer); i++) {
        ITM_SendChar(buffer[i]);
    }
#else
    (void)fmt;
#endif
}

void eif_hal_assert(bool condition, const char* msg) {
    if (!condition) {
        eif_hal_debug_print("ASSERT: %s\n", msg);
        __disable_irq();
        while (1) {
            __BKPT(0);  // Halt debugger
        }
    }
}

// =============================================================================
// Hardware Info
// =============================================================================

uint32_t eif_hal_get_cpu_freq(void) {
    return SystemCoreClock;
}

const char* eif_hal_get_platform(void) {
#if defined(__ARM_ARCH_7EM__)
    return "ARM Cortex-M4F";
#elif defined(__ARM_ARCH_7M__)
    return "ARM Cortex-M3";
#elif defined(__ARM_ARCH_6M__)
    return "ARM Cortex-M0";
#else
    return "ARM Cortex-M";
#endif
}

bool eif_hal_has_simd(void) {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return true;
#else
    return false;
#endif
}

bool eif_hal_has_fpu(void) {
#if defined(__ARM_FP) || defined(__VFP_FP__)
    return true;
#else
    return false;
#endif
}

// =============================================================================
// Cycle Counter (DWT)
// =============================================================================

void eif_hal_cycle_counter_start(void) {
#if HAS_CMSIS && defined(DWT)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

uint32_t eif_hal_cycle_counter_read(void) {
#if HAS_CMSIS && defined(DWT)
    return DWT->CYCCNT;
#else
    return 0;
#endif
}

void eif_hal_cycle_counter_stop(void) {
#if HAS_CMSIS && defined(DWT)
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
#endif
}

// =============================================================================
// Power
// =============================================================================

void eif_hal_sleep(void) {
    __WFI();  // Wait For Interrupt
}

void eif_hal_deep_sleep(void) {
#if HAS_CMSIS
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
#endif
}

#endif // EIF_HAL_PLATFORM_ARM
