/**
 * @file    sample_register.h
 * @brief   SAMPLE_REGISTER macro — each sample file uses it at the end to define a global SampleEntry.
 *
 *   Usage (put at the end of the sample .c file):
 *       SAMPLE_REGISTER(camera, "Camera: photo/video/download + OSD/State push", camera_run);
 *
 *   It merely defines an ordinary exported global variable `const SampleEntry sample_camera`; the symbol name
 *   is fixed as `sample_<id>`, matching `SAMPLE(camera)` in sample_list.inc.
 *   No section / no constructor — any C toolchain digests it.
 */
#ifndef SAMPLE_REGISTER_H
#define SAMPLE_REGISTER_H

#include "sample_registry.h"

#define SAMPLE_REGISTER(id, brief_str, run_fn)              \
    const SampleEntry sample_##id = {                       \
        .name = #id, .brief = (brief_str),                 \
        .run = (run_fn), .available = 0,                   \
        .kind = SAMPLE_KIND_ACTION }

/* SERVICE variant: mark a sample as a long-running, concurrency-safe service (widget, upgrade),
 * eligible for background auto-start by the supervisor. Everything else is identical. */
#define SAMPLE_REGISTER_SERVICE(id, brief_str, run_fn)      \
    const SampleEntry sample_##id = {                       \
        .name = #id, .brief = (brief_str),                 \
        .run = (run_fn), .available = 0,                   \
        .kind = SAMPLE_KIND_SERVICE }

#endif /* SAMPLE_REGISTER_H */
