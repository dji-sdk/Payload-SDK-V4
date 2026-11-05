/**
 * @file    main.c
 * @brief   Linux entry point —— main() calls sample_launcher_run().
 *
 *   This file is the platform entry point. The launcher library code (sample_launcher_run and helpers)
 *   is in samples/launcher/sample_launcher.c.
 */
#include "sample_launcher.h"

int main(int argc, char **argv)
{
    return sample_launcher_run(argc, argv);
}
