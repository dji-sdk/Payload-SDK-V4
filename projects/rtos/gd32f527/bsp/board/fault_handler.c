/**
 * @file    fault_handler.c
 * @brief   Cortex-M33 fault diagnostic dump — implementation.
 *
 * See fault_handler.h for the detailed description.
 *
 * Implementation notes:
 *   - All 4 fault handlers are naked trampolines that only do two things:
 *     1) Select MSP or PSP as the stacking source based on EXC_RETURN.SPSEL (bit 2)
 *     2) Pass sp + EXC_RETURN + a static tag string to dump_and_halt
 *   - dump_and_halt is an ordinary C function: it reads sp[0..7] to get the stacked R0/R1/R2/R3/R12/LR/PC/xPSR,
 *     then reads SCB::{HFSR, CFSR, MMFAR, BFAR}, printf's them all, and enters an infinite loop
 *   - Deliberately does not call vTaskSuspendAll / portDISABLE_INTERRUPTS — because at fault time
 *     the system state is already untrustworthy, calling a FreeRTOS API may fault again. printf is best-effort.
 */
#include "gd32f5xx.h"
#include <stdint.h>
#include <stdio.h>

/* ----------------------------------------------------------------------
 * dump_and_halt
 *
 *  sp     [in] the stack pointer in use before the exception was entered (MSP or PSP)
 *  lr_exc [in] the LR value at exception entry (EXC_RETURN, used to infer SPSEL / FPU state)
 *  tag    [in] the string name for each of the 4 handlers (HardFault / MemManage / ...)
 *
 *  Stacking layout (Cortex-M33, without FPU lazy save):
 *    sp[0] = R0
 *    sp[1] = R1
 *    sp[2] = R2
 *    sp[3] = R3
 *    sp[4] = R12
 *    sp[5] = LR     (faulting context return addr; = caller of failed function)
 *    sp[6] = PC     (faulting instruction address)
 *    sp[7] = xPSR
 *
 *  Cortex-M33 SCB key registers:
 *    HFSR   the FORCED bit means another fault escalated up to this one
 *    CFSR   split into three sections:
 *           bit  0..7  = MMFSR (MemManage)
 *           bit  8..15 = BFSR  (BusFault)
 *           bit 16..31 = UFSR  (UsageFault)
 *    MMFAR  MemManage fault address (access rejected by MPU)   valid bit = MMFSR.MMARVALID (bit 7)
 *    BFAR   BusFault fault address (e.g. 0x20100000 seen in this project's board-swap bug) valid bit = BFSR.BFARVALID (bit 15)
 * ---------------------------------------------------------------------- */
static void __attribute__((used, noreturn))
dump_and_halt(uint32_t *sp, uint32_t lr_exc, const char *tag)
{
    uint32_t r0  = sp[0], r1 = sp[1], r2 = sp[2], r3 = sp[3];
    uint32_t r12 = sp[4], lr = sp[5], pc = sp[6], xpsr = sp[7];
    uint32_t cfsr  = SCB->CFSR;
    uint32_t hfsr  = SCB->HFSR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t bfar  = SCB->BFAR;

    printf("\r\n\r\n!!! [FAULT] %s !!!\r\n", tag);
    printf("  PC   = 0x%08lX  (faulting instruction)\r\n", (unsigned long)pc);
    printf("  LR   = 0x%08lX  (return addr of caller)\r\n", (unsigned long)lr);
    printf("  xPSR = 0x%08lX\r\n", (unsigned long)xpsr);
    printf("  R0=0x%08lX R1=0x%08lX R2=0x%08lX R3=0x%08lX\r\n",
           (unsigned long)r0, (unsigned long)r1, (unsigned long)r2, (unsigned long)r3);
    printf("  R12=0x%08lX SP=%p EXC_RET=0x%08lX\r\n",
           (unsigned long)r12, (void *)sp, (unsigned long)lr_exc);
    printf("  CFSR=0x%08lX HFSR=0x%08lX\r\n",
           (unsigned long)cfsr, (unsigned long)hfsr);
    printf("  MMFAR=0x%08lX (valid=%d)  BFAR=0x%08lX (valid=%d)\r\n",
           (unsigned long)mmfar, (cfsr & (1U << 7))  ? 1 : 0,
           (unsigned long)bfar,  (cfsr & (1U << 15)) ? 1 : 0);

    if (cfsr & 0x000000FFU) {
        printf("  MMFSR: %s%s%s%s%s%s\r\n",
               (cfsr & (1U<<0)) ? "IACCVIOL " : "",
               (cfsr & (1U<<1)) ? "DACCVIOL " : "",
               (cfsr & (1U<<3)) ? "MUNSTKERR " : "",
               (cfsr & (1U<<4)) ? "MSTKERR " : "",
               (cfsr & (1U<<5)) ? "MLSPERR " : "",
               (cfsr & (1U<<7)) ? "MMARVALID " : "");
    }
    if (cfsr & 0x0000FF00U) {
        printf("  BFSR: %s%s%s%s%s%s\r\n",
               (cfsr & (1U<<8))  ? "IBUSERR "      : "",
               (cfsr & (1U<<9))  ? "PRECISERR "    : "",
               (cfsr & (1U<<10)) ? "IMPRECISERR "  : "",
               (cfsr & (1U<<11)) ? "UNSTKERR "     : "",
               (cfsr & (1U<<12)) ? "STKERR "       : "",
               (cfsr & (1U<<15)) ? "BFARVALID "    : "");
    }
    if (cfsr & 0xFFFF0000U) {
        printf("  UFSR: %s%s%s%s%s%s\r\n",
               (cfsr & (1U<<16)) ? "UNDEFINSTR " : "",
               (cfsr & (1U<<17)) ? "INVSTATE "   : "",
               (cfsr & (1U<<18)) ? "INVPC "      : "",
               (cfsr & (1U<<19)) ? "NOCP "       : "",
               (cfsr & (1U<<24)) ? "UNALIGNED "  : "",
               (cfsr & (1U<<25)) ? "DIVBYZERO "  : "");
    }
    printf("!!! halted, please reset board !!!\r\n");
    while (1) { __NOP(); }
}

/* ----------------------------------------------------------------------
 * Naked entry — selects the sp source (MSP/PSP), passes sp + EXC_RETURN to dump_and_halt.
 *
 * tst lr, #4    test EXC_RETURN.SPSEL (the stack used at exception entry):
 *               4 = PSP (task context)
 *               0 = MSP (kernel/handler context)
 * ite/mrseq/mrsne  load MSP or PSP into r0 based on the Z flag
 * mov r1, lr    pass the original EXC_RETURN value to dump (which prints it for debugging)
 * ldr r2, =1f   tag string address
 * b dump_and_halt   tail call (ordinary C ABI, r0/r1/r2 are arguments)
 * 1: .asciz tag
 * ---------------------------------------------------------------------- */
#define FAULT_HANDLER_ENTRY(_name, _tag)                                 \
    void _name(void) __attribute__((naked));                             \
    void _name(void)                                                     \
    {                                                                    \
        __asm volatile (                                                 \
            "tst   lr, #4              \n"                               \
            "ite   eq                  \n"                               \
            "mrseq r0, msp             \n"                               \
            "mrsne r0, psp             \n"                               \
            "mov   r1, lr              \n"                               \
            "ldr   r2, =1f             \n"                               \
            "b     dump_and_halt       \n"                               \
            "1: .asciz \"" _tag "\"    \n"                               \
            ".align 2                  \n");                             \
    }

FAULT_HANDLER_ENTRY(HardFault_Handler,  "HardFault")
FAULT_HANDLER_ENTRY(MemManage_Handler,  "MemManage")
FAULT_HANDLER_ENTRY(BusFault_Handler,   "BusFault")
FAULT_HANDLER_ENTRY(UsageFault_Handler, "UsageFault")
