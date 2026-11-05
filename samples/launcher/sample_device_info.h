/*
 * sample_device_info.h — device report info (central place for users to edit).
 *
 * All samples periodically report device_info using the values here via the shared report module
 * sample_device_report (device->cloud, /state, wire method "device_info"). To change device name /
 * serial number / firmware version / device type / capability list / report period, edit this file
 * only, no need to touch any sample code.
 *
 * Note: device_id is not here — it comes from the runtime onboarding SN (cfg.device_sn / --sn),
 *       passed in by sample_device_report_start().
 *       The device name SAMPLE_DEVICE_NAME is a "default", overridable at runtime (priority:
 *       --name arg > env var THING_MODEL_DEVICE_NAME > this macro), convenient for multi-payload onboarding tests.
 */
#ifndef SAMPLE_DEVICE_INFO_H
#define SAMPLE_DEVICE_INFO_H

/* ── Device meta info (fill in per the actual product) ── */
#ifndef SAMPLE_DEVICE_NAME
#define SAMPLE_DEVICE_NAME             "psdk_test"          /* device name (default, overridable via --name / THING_MODEL_DEVICE_NAME) */
#endif
#define SAMPLE_DEVICE_SERIAL_NUMBER    "SN0358345345"   /* serial number */
#ifndef SAMPLE_DEVICE_FIRMWARE_VERSION
#define SAMPLE_DEVICE_FIRMWARE_VERSION "01.00.00.00"
#endif
#define SAMPLE_DEVICE_TYPE             1                /* device type, see DeviceType in the common schema */

/* Capability list: add/remove strings as needed (comma-separated, count auto-tallied). */
#define SAMPLE_DEVICE_CAPABILITIES \
    "cumstom_hms", "fts"

/* Report period (milliseconds). device_info is low-frequency state, 1Hz is plenty. */
#define SAMPLE_DEVICE_REPORT_PERIOD_MS 1000

#endif /* SAMPLE_DEVICE_INFO_H */
