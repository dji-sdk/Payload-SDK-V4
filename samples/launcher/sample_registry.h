/**
 * @file    sample_registry.h
 * @brief   Single-bin sample integration — the registry's external interface.
 *
 *   Each sample (camera / efts / power / ...) turns its former main body into a uniform-signature
 *   `*_run(const SampleCtx*)`, and at the end of the file uses SAMPLE_REGISTER() to expose a
 *   global `const SampleEntry sample_<id>`. Which samples go into the firmware is decided by the
 *   committed `sample_list.inc` (see that file). The launcher enumerates/finds/dispatches via this interface.
 *
 *   Pure ISO C: no dependency on linker sections or constructors; works with any C toolchain and build system.
 *   Any C toolchain (GCC/Clang/IAR/Keil) + any build system (CMake/Make/IDE) works directly.
 */
#ifndef SAMPLE_REGISTRY_H
#define SAMPLE_REGISTRY_H

#include <stdbool.h>
#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sample run context: replaces each demo's own argc/argv + self-loaded cfg.
 *   - cfg : the session config the launcher already loaded (a sample may copy it locally then override as needed, e.g. --sn).
 *   - argc/argv : already tokenized args. Linux direct mode = argv+1; menu mode = the whole line split into tokens.
 *                 By convention argv[0] = sample name, argv[1..] = the sample's own args.
 */
typedef struct {
    const ThingModelClientSession_Config_t *cfg;
    /* Shared session owned by the launcher (opened once for the whole process). When non-NULL and the
     * sample does not override identity (--sn), the sample reuses it via sample_session_acquire() instead
     * of connecting its own — so switching samples never re-dials the broker. NULL (direct/CI mode) =
     * the sample opens and closes its own session, exactly as before. */
    ThingModelClientSession_t *session;
    /* True when the supervisor runs this sample as a resident background service (vs a foreground
     * action or a direct CLI run). Services use it to skip interactive-only preamble such as the
     * no-broker Phase-A dry-run, which would otherwise spam the log on every boot. */
    bool   background;
    int    argc;
    char **argv;
} SampleCtx;

typedef int  (*SampleRunFn)(const SampleCtx *ctx);
typedef bool (*SampleAvailFn)(void);   /* reserved: returning false hides it from the menu; all NULL=always available this cycle */

/* A sample's role, declared at registration (a property of the sample, not launch policy):
 *   ACTION  — runs and returns (camera shot, efts one-shot, ...); dispatched in the foreground.
 *   SERVICE — long-running, resident, concurrency-safe (widget, upgrade, ...); may be auto-started
 *             and run concurrently in the background on the shared session by the supervisor. */
typedef enum {
    SAMPLE_KIND_ACTION = 0,   /* default (zero) — the common case */
    SAMPLE_KIND_SERVICE = 1,
} SampleKind;

typedef struct {
    const char   *name;       /* command name & menu key, e.g. "camera" */
    const char   *brief;      /* one-line menu blurb */
    SampleRunFn   run;
    SampleAvailFn available;  /* may be NULL */
    SampleKind    kind;       /* ACTION (default) or SERVICE */
} SampleEntry;

/* Enumeration interface (impl in sample_registry.c, hides sample_list.inc details). */
int                sample_registry_count(void);
const SampleEntry *sample_registry_get(int i);
const SampleEntry *sample_registry_find(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_REGISTRY_H */
