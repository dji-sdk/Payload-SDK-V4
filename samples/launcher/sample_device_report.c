/*
 * sample_device_report.c — shared device report implementation (background thread periodically pushes device_info).
 *
 * Reuses the business session's DeviceInfo publisher (no second MQTT connection needed). Device meta info is taken from
 * sample_device_info.h. The background thread splits SAMPLE_DEVICE_REPORT_PERIOD_MS into 100ms naps,
 * so it can finish promptly on stop / sample_should_stop.
 *
 * Thread / sleep all go through the OSAL vtable (Linux=pthread, RTOS=FreeRTOS), shared by both ends.
 */
#include "sample_device_report.h"
#include "sample_device_info.h"
#include "sample_config.h"      /* thing_model_sample_getenv (device name runtime override) */
#include "sample_lifecycle.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"

#include "device_info/device_info_publisher.h"
#include "device_info/device_info_types.h"

#include <string.h>
#include <stdio.h>

/* Capability list comes from the header, count auto-tallied. */
static const char *const s_caps[] = { SAMPLE_DEVICE_CAPABILITIES };
#define SAMPLE_DEVICE_CAP_COUNT ((int)(sizeof(s_caps) / sizeof(s_caps[0])))

static DeviceInfo_Publisher_t *s_pub      = NULL;
static char                    s_device_id[128];
static char                    s_device_name[128];   /* resolved at runtime: arg > env var > macro */
static char                    s_device_serial[128];
static volatile int            s_running   = 0;
static void                   *s_thread    = NULL;   /* OSAL thread handle */

/* Fill one device_info frame with values from sample_device_info.h (device_id / device_name are runtime-resolved values). */
static void fill_push(T_DeviceInfoPush *push)
{
    memset(push, 0, sizeof(*push));
    push->device_id          = s_device_id;
    push->device_name        = s_device_name;
    push->serial_number      = s_device_serial;
    push->firmware_version   = (char *)SAMPLE_DEVICE_FIRMWARE_VERSION;
    push->device_type        = SAMPLE_DEVICE_TYPE;
    push->capabilities       = (char **)s_caps;
    push->capabilities_count = (size_t)SAMPLE_DEVICE_CAP_COUNT;
}

static void report_loop(void *arg)
{
    (void)arg;
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    while (s_running && !sample_should_stop()) {
        T_DeviceInfoPush push;
        fill_push(&push);
        DeviceInfo_PublishInfo(s_pub, &push);

        /* Split the period into 100ms naps, so it responds promptly to stop / Ctrl-C. */
        int slept = 0;
        while (s_running && !sample_should_stop() && slept < SAMPLE_DEVICE_REPORT_PERIOD_MS) {
            if (osal && osal->sleep_ms) osal->sleep_ms(100);
            slept += 100;
        }
    }
}

int sample_device_report_start(ThingModelClientSession_t *session,
                               const char *device_id, const char *device_name)
{
    if (s_running) return 0;
    if (!session)  return -1;

    const T_ThingModelOsal *osal = ThingModelOsal_Get();

    snprintf(s_device_id, sizeof(s_device_id), "%s", device_id ? device_id : "");

    /* Device-name resolution priority: explicit arg > env var THING_MODEL_DEVICE_NAME > macro SAMPLE_DEVICE_NAME. */
    const char *name = (device_name && *device_name)
                       ? device_name
                       : thing_model_sample_getenv("THING_MODEL_DEVICE_NAME", SAMPLE_DEVICE_NAME);
    snprintf(s_device_name, sizeof(s_device_name), "%s", name);
    snprintf(s_device_serial, sizeof(s_device_serial), "%s",
             thing_model_sample_getenv("THING_MODEL_DEVICE_SERIAL_NUMBER", SAMPLE_DEVICE_SERIAL_NUMBER));

    s_pub = DeviceInfo_CreatePublisher(session);
    if (!s_pub) return -1;

    s_running = 1;
    s_thread = osal && osal->thread_create
        ? osal->thread_create(report_loop, NULL, "dev_report", 0)
        : NULL;
    if (!s_thread) {
        s_running = 0;
        DeviceInfo_DestroyPublisher(s_pub);
        s_pub = NULL;
        return -1;
    }
    LOG_INFO("[device_report] device report started: name=%s id=%s serial=%s @ %dms (%d capabilities)",
             s_device_name, s_device_id, s_device_serial, SAMPLE_DEVICE_REPORT_PERIOD_MS, SAMPLE_DEVICE_CAP_COUNT);
    return 0;
}

void sample_device_report_stop(void)
{
    if (!s_running) return;
    s_running = 0;
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    if (osal && osal->thread_destroy && s_thread)
        osal->thread_destroy(s_thread);
    s_thread = NULL;
    if (s_pub) {
        DeviceInfo_DestroyPublisher(s_pub);
        s_pub = NULL;
    }
}
