/**
 * @file    sample_console_posix.c
 * @brief   Linux/POSIX implementation of menu-selection character reading. On MCU use sample_console_uart.c instead.
 *
 *   MCU version reference (pseudo-code):
 *     int sample_console_getch(void){ uint8_t b; return uart_read(&b,1)==1 ? b : -1; }
 */
#include "sample_console.h"

#include <stdio.h>

int sample_console_getch(void)
{
    int c = getchar();
    if (c == EOF) return -1;        /* Ctrl-D / pipe closed / Ctrl-C interrupt → exit menu */
    /* Terminal is line-buffered: user enters "1<Enter>", discard the rest of this line (including newline). */
    if (c != '\n') {
        int d;
        while ((d = getchar()) != EOF && d != '\n') { /* drain */ }
    }
    return c;
}
