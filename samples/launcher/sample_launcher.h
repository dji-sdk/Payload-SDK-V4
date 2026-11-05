/**
 * @file    sample_launcher.h
 * @brief   Sample launcher entry — for platforms that "provide their own main / RTOS task" (e.g. MCU).
 *
 *   Linux: this project provides main() by default, just run it (see sample_main.c).
 *   MCU/RTOS: your project already has its own main/scheduler; use -DSAMPLE_NO_MAIN to turn off this
 *             project's main(), and in your task, after platform init, call sample_launcher_run():
 *
 *       // 1) Register your OSAL (the POSIX launcher auto-registers; MCU provides its own)
 *       ThingModelOsal_Register(&my_rtos_osal);
 *       // 2) (optional) Register your transport/crypto vtable (bring-your-own implementation)
 *       // 3) Run samples: combine with -DSAMPLE_AUTORUN=all to run all cases
 *       sample_launcher_run(0, NULL);
 *
 *   Note: sample_launcher_run internally auto-registers OSAL only on POSIX platforms; non-POSIX platforms
 *   assume the caller registered OSAL (and any required transport/crypto) before the call.
 */
#ifndef SAMPLE_LAUNCHER_H
#define SAMPLE_LAUNCHER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Launcher main flow: load config -> select and dispatch a sample per the front door (menu / autorun).
 * @param argc/argv  command-line args; on MCU with no args, pass (0, NULL).
 * @return process/task return code (0 = normal).
 */
int sample_launcher_run(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_LAUNCHER_H */
