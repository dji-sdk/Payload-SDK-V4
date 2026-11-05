/*
 * sample_device_report.h — shared device report (used by all samples).
 *
 * After each sample connects to the broker, it starts a background thread that periodically pushes
 * device_info to the cloud (device->cloud, /state, wire method "device_info"). The device meta info is
 * centralized in sample_device_info.h; edit that file only, no need to touch this module or any sample.
 *
 * Typical usage (one line each after Connect succeeds and before Disconnect in every sample):
 *   ThingModelClientSession_t *s = ThingModelClientSession_Connect(&cfg);
 *   sample_device_report_start(s, cfg.device_sn, NULL);
 *   ...
 *   sample_device_report_stop();
 *   ThingModelClientSession_Disconnect(s);
 *
 * start/stop are idempotent: repeated start is ignored, stop is a no-op when not started.
 */
#ifndef SAMPLE_DEVICE_REPORT_H
#define SAMPLE_DEVICE_REPORT_H

#include "thing_model_client/thing_model_client_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the device-report background thread (period see SAMPLE_DEVICE_REPORT_PERIOD_MS in sample_device_info.h).
 *
 * Both device_id / device_name can be specified at runtime, convenient for multi-payload onboarding tests (one identity per process):
 *   · device_id   pass cfg.device_sn (already overridable via --sn / THING_MODEL_DEVICE_SN).
 *   · device_name when NULL/"" is passed, falls back to env var THING_MODEL_DEVICE_NAME, then to
 *                 the SAMPLE_DEVICE_NAME macro in sample_device_info.h. A non-empty value is used directly.
 *
 * @param session      an already-Connected session
 * @param device_id    the device_id used for reporting (usually = cfg.device_sn)
 * @param device_name  the device name used for reporting; NULL/"" = use THING_MODEL_DEVICE_NAME or macro default
 * @return 0 on success; non-0 on failure (also returns 0 if already running).
 */
int  sample_device_report_start(ThingModelClientSession_t *session,
                                const char *device_id, const char *device_name);

/** Stop the report thread and reclaim the publisher (no-op if not started). */
void sample_device_report_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_DEVICE_REPORT_H */
