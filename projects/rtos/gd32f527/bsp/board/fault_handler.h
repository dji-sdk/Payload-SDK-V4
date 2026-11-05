/**
 * @file    fault_handler.h
 * @brief   Cortex-M33 fault diagnostic dump — public API.
 *
 *   Provides four fault entry points:
 *     HardFault_Handler / MemManage_Handler / BusFault_Handler / UsageFault_Handler
 *   Each is a naked trampoline that parses the hardware-stacked R0-R3 / R12 / LR / PC / xPSR,
 *   plus SCB::{HFSR, CFSR, MMFAR, BFAR}, printf's them all, then infinite-loops.
 *
 *   Diagnostic flow:
 *     1. Get PC = 0xXXXXXXXX from the serial port
 *     2. Run arm-none-eabi-addr2line -e <elf> -f -C 0xXXXXXXXX
 *     3. Immediately know which function at which line triggered the fault
 *     4. The CFSR decoded fields tell you whether it is IBUS / DBUS / unaligned / div0 / undefined instruction …
 *
 *   How a sample project uses it:
 *     - Compile fault_handler.c into the bsp library (samples add it by default)
 *     - Do not define a handler of the same name again in gd32f5xx_it.c, otherwise multiple definition
 *     - No need to include this header in main.c — the handlers are ISR vector strong symbols,
 *       the linker automatically overrides the weak alias at link time
 *
 *   Dependencies:
 *     - stdio.h::printf (UART is ready)
 *     - cmsis::SCB
 *     - cmsis::__NOP
 *
 *   Real-world case to cite when writing docs:
 *     - board-swap hardfault, BFAR=0x20100000, PC inside memcpy
 *       → addr2line traces LR back to enet_data_to_usb
 *       → root cause: USB-RNDIS class_data is NULL (packet sent before USB ready)
 *     See docs/rtos-gd32-port.md §3.5 for details
 */
#ifndef FAULT_HANDLER_H
#define FAULT_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

/* No public API — handlers are ISR vector strong symbols, linker auto-binds.
 * This header exists only for IDE navigation / documentation anchor. */

#ifdef __cplusplus
}
#endif
#endif /* FAULT_HANDLER_H */
