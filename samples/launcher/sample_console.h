/**
 * @file    sample_console.h
 * @brief   Platform abstraction for menu selection — the only (and minimal) porting point.
 *
 *   Only "read one character" is needed, no line editing/buffering:
 *     Linux impl: sample_console_posix.c (getchar).
 *     MCU  impl: write a separate sample_console_uart.c, reading 1 byte from UART (about 2~3 lines).
 *
 *   If you don't even want to adapt this one character, use compile-time autorun (see
 *   SAMPLE_AUTORUN / SAMPLE_AUTORUN_ALL in sample_main.c): this interface is not called at all, no console compiled.
 *
 *   Note: sample and menu "output" use printf directly — on MCU, printf is conventionally already redirected to
 *   UART, so no output abstraction is needed; this interface only abstracts the platform-specific "read one selection char".
 */
#ifndef SAMPLE_CONSOLE_H
#define SAMPLE_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Blocking read of one input character (for menu selection, e.g. '1'..'9' / 'q').
 * @return the character read; returns -1 on end of input / interruption (e.g. Ctrl-C) (launcher exits the menu accordingly).
 */
int sample_console_getch(void);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_CONSOLE_H */
