/**
 * @file    data_subscription_client_demo.c
 * @brief   sample_things_model/data_subscription — shared by Linux POSIX and RTOS FreeRTOS.
 *
 * Flight-controller data subscription interface demo. Protocol source: FC telemetry data subscription doc
 *   (fc_subscribe_topic / fc_unsubscribe_topic / fc_get_latest_value / fc_publish_data)
 *
 * Third-party deps (Linux) are all vendored under ../../third_party/ (libmosquitto / libssl /
 * libcrypto prebuilt .so + cJSON source); deployed to the device as: bin/sample_things_model + lib/*.so.
 *
 * Demonstrates 3 SERVICEs + 1 OSD push, subscribing all 38 topics:
 *   Phase A (dry-run, no broker needed):
 *     - SERVICE Build*Payload — subscribe_topic / unsubscribe_topic / get_latest_value
 *     - OSD Topic Build       — OSD topic string
 *   Phase B (broker required):
 *     1. Subscribe all 38 topics
 *     2. Raw listener: receive fc_publish_data, manually parse each topic's data and print it
 *     3. Run an observation window (THING_MODEL_OBSERVE_SEC seconds, 0=indefinite) counting messages received per topic
 *     4. GetLatestValue demo: issue a query on the quaternion topic, wait for the OSD push response
 *     5. Unsubscribe all subscribed topics
 *
 * Platform split (same as custom_small_data):
 *   Linux (POSIX): argv/getenv parse --sn/--verbose/--debug, spawn a background thread for the sleep observation loop
 *   RTOS:          no argv/env, use hardcoded defaults (verbose=off, observe=30s). sleep uses
 *                  the OSAL vtable; sem_t replaced by OSAL event; no interactive Phase A dry-run printing.
 *
 * Key abstraction: core logic is centralized in data_subscription_run_with_session(), shared by both ends:
 *   - Linux SAMPLE_REGISTER callback → sample_session_open() → data_subscription_run_with_session
 *   - RTOS  rtos_all wraps the sample → directly calls data_subscription_run_with_session(shared_session)
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_log.h"
#include "data_subscription/data_subscription_client.h"
#include "data_subscription/data_subscription_types.h"
#include "data_subscription/data_subscription_json.h"
#include "data_subscription/data_subscription_topics.h"
#include "common/common_types.h"

#include <cJSON.h>   /* vendored: samples/third_party/cJSON/cJSON.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Platform split (same pattern as custom_small_data)
 * ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define DS_PLATFORM_POSIX 1
#  include "sample_config.h"
#  include "sample_register.h"
#  include "sample_lifecycle.h"
#  include "sample_session.h"
#  include "sample_aircraft_info.h"   /* sample_aircraft_info_set_enabled() */
#else
#  define DS_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#  include "sample_lifecycle.h"   /* sample_should_stop() — also provided on the RTOS end, always returns false */
#endif

/* ============================================================
 * Full set of subscription topics (39 total, grouped by DJI_FC_SUBSCRIPTION_TOPIC_*)
 * Note: DJI_FC_SUBSCRIPTION_TOPIC_BATTERY_INFO (whole-aircraft aggregate) is unsupported and has been removed.
 * ============================================================ */

/* --- 1. IMU / attitude (10 Hz) --- */
#define TOPIC_QUATERNION          "quaternion"
#define TOPIC_ACCEL_GROUND        "acceleration_ground"
#define TOPIC_ACCEL_BODY          "acceleration_body"
#define TOPIC_ACCEL_RAW           "acceleration_raw"
#define TOPIC_VELOCITY            "velocity"
#define TOPIC_ANG_RATE_FUSED      "angular_rate_fused"
#define TOPIC_ANG_RATE_RAW        "angular_rate_raw"
/* --- 2. altitude (1 Hz) --- */
#define TOPIC_ALT_FUSED           "altitude_fused"
#define TOPIC_ALT_BARO            "altitude_barometer"
#define TOPIC_ALT_HOMEPOINT       "altitude_of_homepoint"
#define TOPIC_HEIGHT_FUSION       "height_fusion"
/* --- 3. position --- */
#define TOPIC_POS_FUSED           "position_fused"       /* 1 Hz  */
#define TOPIC_POS_VO              "position_vo"          /* 10 Hz */
/* --- 4. GPS (1 Hz) --- */
#define TOPIC_GPS_DATE            "gps_date"
#define TOPIC_GPS_TIME            "gps_time"
#define TOPIC_GPS_POSITION        "gps_position"
#define TOPIC_GPS_VELOCITY        "gps_velocity"
#define TOPIC_GPS_DETAILS         "gps_details"
#define TOPIC_GPS_SIGNAL          "gps_signal_level"
/* --- 5. RTK (1 Hz) --- */
#define TOPIC_RTK_POSITION        "rtk_position"
#define TOPIC_RTK_VELOCITY        "rtk_velocity"
#define TOPIC_RTK_YAW             "rtk_yaw"
#define TOPIC_RTK_POSITION_INFO   "rtk_position_info"
#define TOPIC_RTK_YAW_INFO        "rtk_yaw_info"
#define TOPIC_RTK_CONNECT         "rtk_connect_status"
/* --- 6. RC (10 Hz) --- */
#define TOPIC_RC                  "rc"
#define TOPIC_RC_FLAG             "rc_with_flag_data"
/* --- 7. flight status (1 Hz) --- */
#define TOPIC_STATUS_FLIGHT       "status_flight"
#define TOPIC_STATUS_DISPLAYMODE  "status_displaymode"
/* status_motor_start_error (DJI_FC_SUBSCRIPTION_TOPIC_STATUS_MOTOR_START_ERROR) unsupported, removed */
/* --- 8. control device (1 Hz) --- */
#define TOPIC_CONTROL_DEVICE      "control_device"
/* --- 9. sensors / safety --- */
#define TOPIC_COMPASS             "compass"              /* 10 Hz */
#define TOPIC_ESC_DATA            "esc_data"             /* 10 Hz */
#define TOPIC_FLIGHT_ANOMALY      "flight_anomaly"       /* 1  Hz */
#define TOPIC_AVOID_DATA          "avoid_data"           /* 10 Hz */
/* --- 10. home point (1 Hz) --- */
#define TOPIC_HOME_POINT_STATUS   "home_point_set_status"
#define TOPIC_HOME_POINT_INFO     "home_point_info"
/* --- 11. battery (1 Hz) --- */
#define TOPIC_BATTERY_SINGLE1     "battery_single_info_index1"
#define TOPIC_BATTERY_SINGLE2     "battery_single_info_index2"

/* Subscription config: {topic name, frequency (Hz)} */
static const struct { const char *name; int freq; } g_topics[] = {
    /* 1 */ { TOPIC_QUATERNION,         10 },
    /* 2 */ { TOPIC_ACCEL_GROUND,       10 },
    /* 3 */ { TOPIC_ACCEL_BODY,         10 },
    /* 4 */ { TOPIC_ACCEL_RAW,          10 },
    /* 5 */ { TOPIC_VELOCITY,           10 },
    /* 6 */ { TOPIC_ANG_RATE_FUSED,     10 },
    /* 7 */ { TOPIC_ANG_RATE_RAW,       10 },
    /* 8 */ { TOPIC_ALT_FUSED,           1 },
    /* 9 */ { TOPIC_ALT_BARO,            1 },
    /*10 */ { TOPIC_ALT_HOMEPOINT,       1 },
    /*11 */ { TOPIC_HEIGHT_FUSION,       1 },
    /*12 */ { TOPIC_POS_FUSED,           1 },
    /*13 */ { TOPIC_POS_VO,             10 },
    /*14 */ { TOPIC_GPS_DATE,            1 },
    /*15 */ { TOPIC_GPS_TIME,            1 },
    /*16 */ { TOPIC_GPS_POSITION,        1 },
    /*17 */ { TOPIC_GPS_VELOCITY,        1 },
    /*18 */ { TOPIC_GPS_DETAILS,         1 },
    /*19 */ { TOPIC_GPS_SIGNAL,          1 },
    /*20 */ { TOPIC_RTK_POSITION,        1 },
    /*21 */ { TOPIC_RTK_VELOCITY,        1 },
    /*22 */ { TOPIC_RTK_YAW,             1 },
    /*23 */ { TOPIC_RTK_POSITION_INFO,   1 },
    /*24 */ { TOPIC_RTK_YAW_INFO,        1 },
    /*25 */ { TOPIC_RTK_CONNECT,         1 },
    /*26 */ { TOPIC_RC,                 10 },
    /*27 */ { TOPIC_RC_FLAG,            10 },
    /*28 */ { TOPIC_STATUS_FLIGHT,       1 },
    /*29 */ { TOPIC_STATUS_DISPLAYMODE,  1 },
    /*30 */ { TOPIC_CONTROL_DEVICE,      1 },
    /*31 */ { TOPIC_COMPASS,            10 },
    /*32 */ { TOPIC_ESC_DATA,           10 },
    /*33 */ { TOPIC_FLIGHT_ANOMALY,      1 },
    /*34 */ { TOPIC_AVOID_DATA,         10 },
    /*35 */ { TOPIC_HOME_POINT_STATUS,   1 },
    /*36 */ { TOPIC_HOME_POINT_INFO,     1 },
    /*37 */ { TOPIC_BATTERY_SINGLE1,     1 },
    /*38 */ { TOPIC_BATTERY_SINGLE2,     1 },
};
#define TOPIC_COUNT ((int)(sizeof(g_topics) / sizeof(g_topics[0])))

/* Batch subscription: BATCH_SIZE per batch, BATCH_ITEM_MS between items in a batch, BATCH_GAP_MS between batches.
 * Avoids overwhelming the FC fc_subscription service with a burst of subscription requests. */
#define BATCH_SIZE    5
#define BATCH_ITEM_MS 80
#define BATCH_GAP_MS  300

/* ============================================================
 * Global statistics and control
 * ============================================================ */
static volatile int  g_verbose       = 0;  /* --verbose mode */
static volatile int  g_counts[TOPIC_COUNT]; /* messages received per topic */
static uint64_t      g_last_print_ms[TOPIC_COUNT];
static const char   *g_debug_filter  = NULL; /* --debug <topic> filter string, NULL=no filter */
static void         *g_latest_event  = NULL;  /* GetLatestValue response event (OSAL) */
static volatile int  g_latest_done   = 0;  /* whether the GetLatestValue response was received */
static int32_t       g_raw_listener  = -1; /* raw listener handle */

#define DS_RTOS_PRINT_INTERVAL_MS 300000U

/* ============================================================
 * Platform-agnostic sleep / event convenience wrappers — go through the current session's OSAL vtable.
 * Shared by both ends; on Linux the vtable is a pthread/pipe implementation, on RTOS a FreeRTOS one.
 * ============================================================ */
static void ds_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }

/* stop gate — on Linux uses the launcher's sample_should_stop() (SIGINT/deadline),
 * on RTOS the launcher also has sample_should_stop but it always returns false (extern
 * declaration at the top). Unifying on sample_should_stop for both ends is the simplest. */
static inline bool ds_should_stop(void)
{
    return sample_should_stop();
}

static bool ds_should_print_topic(int index)
{
    if (!g_verbose) return false;
#if DS_PLATFORM_POSIX
    (void)index;
    return true;
#else
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    uint64_t now = (osal && osal->time_ms) ? osal->time_ms() : 0;
    if (g_counts[index] == 1 || now - g_last_print_ms[index] >= DS_RTOS_PRINT_INTERVAL_MS) {
        g_last_print_ms[index] = now;
        return true;
    }
    return false;
#endif
}

/* ============================================================
 * Helper functions: read float / double / int fields from a cJSON object
 * ============================================================ */
static float   json_get_float  (const cJSON *obj, const char *key, float   def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (v && cJSON_IsNumber(v)) ? (float)v->valuedouble : def;
}
static double  json_get_double (const cJSON *obj, const char *key, double  def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (v && cJSON_IsNumber(v)) ? v->valuedouble : def;
}
static int     json_get_int    (const cJSON *obj, const char *key, int     def) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (v && cJSON_IsNumber(v)) ? v->valueint : def;
}

/* ============================================================
 * Print each topic's data (verbose mode)
 *
 * Common helpers:
 *   print_vec3f(d, tag)  — {x,y,z} float vector
 *   print_value(d, tag)  — {"value": X} single value (common format for server single-value topics)
 *   print_generic(d,tag) — print key by key (fallback for unknown/complex formats)
 * ============================================================ */
static void print_vec3f(const cJSON *d, const char *tag) {
    LOG_INFO("[%-28s] x=%.4f y=%.4f z=%.4f", tag,
           json_get_float(d,"x",0.f), json_get_float(d,"y",0.f), json_get_float(d,"z",0.f));
}
static void print_generic(const cJSON *d, const char *tag) {
    char line[512];
    int off = snprintf(line, sizeof(line), "[%-28s]", tag);
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, d) {
        if (!item->string) continue;
        if (off < 0 || off >= (int)sizeof(line)) break;
        if (cJSON_IsNumber(item))       off += snprintf(line+off, sizeof(line)-off, " %s=%.4g", item->string, item->valuedouble);
        else if (cJSON_IsString(item))  off += snprintf(line+off, sizeof(line)-off, " %s=%s",   item->string, item->valuestring);
        else if (cJSON_IsBool(item))    off += snprintf(line+off, sizeof(line)-off, " %s=%d",   item->string, cJSON_IsTrue(item));
        else if (cJSON_IsObject(item))  off += snprintf(line+off, sizeof(line)-off, " %s={...}", item->string);
    }
    LOG_INFO("%s", line);
}
static void print_value(const cJSON *d, const char *tag) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(d, "value");
    if (v && cJSON_IsNumber(v))
        LOG_INFO("[%-28s] %.4g", tag, v->valuedouble);
    else
        print_generic(d, tag);
}

/* ---- 1. IMU / attitude ---- */
static void print_quaternion(const cJSON *d) {
    LOG_INFO("[%-28s] q0=%.4f q1=%.4f q2=%.4f q3=%.4f", "quaternion",
           json_get_float(d,"q0",0.f), json_get_float(d,"q1",0.f),
           json_get_float(d,"q2",0.f), json_get_float(d,"q3",0.f));
}
static void print_accel_ground(const cJSON *d)   { print_vec3f(d, "accel_ground(m/s2)"); }
static void print_accel_body(const cJSON *d)     { print_vec3f(d, "accel_body(m/s2)"); }
static void print_accel_raw(const cJSON *d)      { print_vec3f(d, "accel_raw(m/s2)"); }
static void print_velocity(const cJSON *d) {
    LOG_INFO("[%-28s] x=%.3f y=%.3f z=%.3f m/s health=%d", "velocity",
           json_get_float(d,"x",0.f), json_get_float(d,"y",0.f), json_get_float(d,"z",0.f),
           json_get_int(d,"health",0));
}
static void print_ang_rate_fused(const cJSON *d) { print_vec3f(d, "ang_rate_fused(rad/s)"); }
static void print_ang_rate_raw(const cJSON *d)   { print_vec3f(d, "ang_rate_raw(rad/s)"); }

/* ---- 2. altitude ---- */
static void print_alt_fused(const cJSON *d)      { print_value(d, "alt_fused(m)"); }
static void print_alt_baro(const cJSON *d)       { print_value(d, "alt_baro(m)"); }
static void print_alt_homepoint(const cJSON *d)  { print_value(d, "alt_homepoint(m)"); }
static void print_height_fusion(const cJSON *d)  { print_value(d, "height_fusion(m)"); }

/* ---- 3. position ---- */
static void print_pos_fused(const cJSON *d) {
    LOG_INFO("[%-28s] lon=%.7f lat=%.7f alt=%.2fm sats=%d", "pos_fused",
           json_get_double(d,"longitude",0.0), json_get_double(d,"latitude",0.0),
           json_get_float(d,"altitude",0.f), json_get_int(d,"visibleSatelliteNumber",0));
}
static void print_pos_vo(const cJSON *d) {
    LOG_INFO("[%-28s] x=%.2f(%d) y=%.2f(%d) z=%.2f(%d) m", "pos_vo",
           json_get_float(d,"x",0.f), json_get_int(d,"xHealth",0),
           json_get_float(d,"y",0.f), json_get_int(d,"yHealth",0),
           json_get_float(d,"z",0.f), json_get_int(d,"zHealth",0));
}

/* ---- 4. GPS ---- */
static void print_gps_date(const cJSON *d)      { print_value(d, "gps_date(yyyymmdd)"); }
static void print_gps_time(const cJSON *d)      { print_value(d, "gps_time(hhmmss)"); }
static void print_gps_position(const cJSON *d) {
    LOG_INFO("[%-28s] x=%d y=%d z=%d", "gps_position(deg*1e7/mm)",
           json_get_int(d,"x",0), json_get_int(d,"y",0), json_get_int(d,"z",0));
}
static void print_gps_velocity(const cJSON *d)  { print_vec3f(d, "gps_velocity(cm/s)"); }
static void print_gps_details(const cJSON *d)   { print_generic(d, "gps_details"); }
static void print_gps_signal(const cJSON *d)    { print_value(d, "gps_signal(0-5)"); }

/* ---- 5. RTK ---- */
static void print_rtk_position(const cJSON *d)    { print_vec3f(d, "rtk_position"); }
static void print_rtk_velocity(const cJSON *d)    { print_vec3f(d, "rtk_velocity(cm/s)"); }
static void print_rtk_yaw(const cJSON *d)          { print_value(d, "rtk_yaw(deg)"); }
static void print_rtk_position_info(const cJSON *d){ print_value(d, "rtk_pos_info"); }
static void print_rtk_yaw_info(const cJSON *d)     { print_value(d, "rtk_yaw_info"); }
static void print_rtk_connect(const cJSON *d)      { print_value(d, "rtk_connect(0/1)"); }

/* ---- 6. RC ---- */
static void print_rc(const cJSON *d) {
    LOG_INFO("[%-28s] roll=%d pitch=%d yaw=%d thr=%d mode=%d gear=%d", "rc",
           json_get_int(d,"roll",0), json_get_int(d,"pitch",0), json_get_int(d,"yaw",0),
           json_get_int(d,"throttle",0), json_get_int(d,"mode",0), json_get_int(d,"gear",0));
}
static void print_rc_flag(const cJSON *d) {
    LOG_INFO("[%-28s] pitch=%.3f roll=%.3f yaw=%.3f thr=%.3f"
           " logic=%d sky=%d app=%d gnd=%d", "rc_with_flag",
           json_get_float(d,"pitch",0.f), json_get_float(d,"roll",0.f),
           json_get_float(d,"yaw",0.f), json_get_float(d,"throttle",0.f),
           json_get_int(d,"logic_connected",0), json_get_int(d,"sky_connected",0),
           json_get_int(d,"app_connected",0), json_get_int(d,"ground_connected",0));
}

/* ---- 7. flight status ---- */
static void print_status_flight(const cJSON *d) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(d, "value");
    if (!v || !cJSON_IsNumber(v))
        v = cJSON_GetObjectItemCaseSensitive(d, "status");
    if (!v || !cJSON_IsNumber(v)) {
        print_generic(d, "status_flight");
        return;
    }
    int s = v->valueint;
    LOG_INFO("[%-28s] %d (%s)", "status_flight", s,
            s==0?"ground/motor stopped":s==1?"ground/motor spinning":s==2?"in air":"unknown");
}
static void print_status_displaymode(const cJSON *d){ print_value(d, "status_displaymode"); }
/* print_motor_start_error removed */

/* ---- 8. control device ---- */
/* Non-M300/350 branch: controlAuthority + controlAuthorityChangeReason (see the
 * T_DjiFcSubscriptionControlDevice union in PSDK dji_fc_subscription.h). M300/M350
 * takes the other union branch (controlMode + bit fields); on the wire it's one or the other, not both. */
static void print_control_device(const cJSON *d) {
    LOG_INFO("[%-28s] control_authority=%d change_reason=%d", "control_device",
           json_get_int(d, "control_authority", 0),
           json_get_int(d, "control_authority_change_reason", 0));
}

/* ---- 9. sensors / safety ---- */
static void print_compass(const cJSON *d) {
    LOG_INFO("[%-28s] x=%d y=%d z=%d", "compass",
           json_get_int(d,"x",0), json_get_int(d,"y",0), json_get_int(d,"z",0));
}
static void print_esc_data(const cJSON *d)       { print_generic(d, "esc_data"); }
static void print_flight_anomaly(const cJSON *d) {
    struct { const char *k; const char *l; } f[] = {
        {"impactInAir","collision"},{"randomFly","erratic flight"},{"heightCtrlFail","altitude out of control"},
        {"rollPitchCtrlFail","roll/pitch out of control"},{"yawCtrlFail","yaw out of control"},
        {"aircraftIsFalling","falling"},{"strongWindLevel1","strong wind L1"},{"strongWindLevel2","strong wind L2"},
        {"compassInstallationError","compass installation error"},{"imuInstallationError","IMU installation error"},
        {"escTemperatureHigh","ESC overheat"},{"atLeastOneEscDisconnected","ESC disconnected"},
        {"gpsYawError","GPS heading error"},
    };
    int any = 0;
    char line[512];
    int off = snprintf(line, sizeof(line), "[%-28s]", "flight_anomaly");
    for (int i = 0; i < (int)(sizeof(f)/sizeof(f[0])); i++)
        if (json_get_int(d,f[i].k,0)) {
            if (off >= 0 && off < (int)sizeof(line))
                off += snprintf(line+off, sizeof(line)-off, " [%s]",f[i].l);
            any=1;
        }
    if (off >= 0 && off < (int)sizeof(line))
        snprintf(line+off, sizeof(line)-off, "%s", any ? "" : " normal");
    LOG_INFO("%s", line);
}
static void print_avoid_data(const cJSON *d) {
    LOG_INFO("[%-28s] F=%.1f(%d) B=%.1f(%d) L=%.1f(%d)"
           " R=%.1f(%d) U=%.1f(%d) D=%.1f(%d) m", "avoid_data",
           json_get_float(d,"front",0.f),json_get_int(d,"frontHealth",0),
           json_get_float(d,"back",0.f), json_get_int(d,"backHealth",0),
           json_get_float(d,"left",0.f), json_get_int(d,"leftHealth",0),
           json_get_float(d,"right",0.f),json_get_int(d,"rightHealth",0),
           json_get_float(d,"up",0.f),   json_get_int(d,"upHealth",0),
           json_get_float(d,"down",0.f), json_get_int(d,"downHealth",0));
}

/* ---- 10. home point ---- */
static void print_home_point_status(const cJSON *d){ print_value(d, "home_pt_status(0/1)"); }
static void print_home_point_info(const cJSON *d) {
    LOG_INFO("[%-28s] lon=%.7f lat=%.7f rad", "home_point_info",
           json_get_double(d,"longitude",0.0), json_get_double(d,"latitude",0.0));
}

/* ---- 11. battery ---- */
static void print_battery_single(const cJSON *d, const char *tag) {
    LOG_INFO("[%-28s] idx=%d %d%% volt=%.2fV cur=%.3fA"
           " rem=%d/%dmAh temp=%.1f°C cells=%d", tag,
           json_get_int(d,"battery_index",0), json_get_int(d,"battery_capacity_percent",0),
           json_get_int(d,"current_voltage",0)/1000.0,
           json_get_int(d,"current_electric",0)/1000.0,
           json_get_int(d,"remained_capacity",0), json_get_int(d,"full_capacity",0),
           json_get_int(d,"battery_temperature",0)/10.0, json_get_int(d,"cell_count",0));
}
static void print_battery_single1(const cJSON *d) { print_battery_single(d, "battery1"); }
static void print_battery_single2(const cJSON *d) { print_battery_single(d, "battery2"); }

/* --debug raw capture: print every PUBLISH the session receives (topic + raw payload).
 * When passed --debug <topic>, only print messages whose data object contains that key, to avoid flooding.
 * The callback fires on the transport thread; it only does printf, which is safe. */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    /* When a filter is set: only print messages that contain the given topic key */
    if (g_debug_filter) {
        /* Fast string search (avoids JSON parse overhead): the key appears as "xxx": */
        if (!strstr(payload, g_debug_filter)) return;
    }
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

/* ============================================================
 * Raw message listener: manual parse and dispatch of fc_publish_data
 *
 * This callback is invoked on the transport thread. It only does printf + counting;
 * it does not call sync APIs or Destroy* (forbidden in a transport-thread callback).
 * ============================================================ */
static void on_raw_message(void *user, const char *topic,
                            const char *payload, size_t len)
{
    (void)user;
    (void)len;

    /* Filter out non-OSD topics (only handle .../osd) */
    if (!strstr(topic, "/osd")) return;

    cJSON *root = cJSON_Parse(payload);
    if (!root) return;

    /* Check method == "fc_publish_data" or "fc_get_latest_value" */
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    if (!method || !cJSON_IsString(method)) {
        cJSON_Delete(root);
        return;
    }
    const char *m = method->valuestring;
    int is_publish = (strcmp(m, "fc_publish_data") == 0);
    int is_latest  = (strcmp(m, "fc_get_latest_value") == 0);
    if (!is_publish && !is_latest) {
        cJSON_Delete(root);
        return;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data) {
        cJSON_Delete(root);
        return;
    }

    /* ── Dispatch to each topic's print function (indices strictly matching g_topics[]) ── */
    struct {
        const char  *name;
        int          idx;
        void        (*print)(const cJSON *);
    } dispatch[] = {
        /* 1. IMU / attitude */
        { TOPIC_QUATERNION,          0,  print_quaternion        },
        { TOPIC_ACCEL_GROUND,        1,  print_accel_ground      },
        { TOPIC_ACCEL_BODY,          2,  print_accel_body        },
        { TOPIC_ACCEL_RAW,           3,  print_accel_raw         },
        { TOPIC_VELOCITY,            4,  print_velocity          },
        { TOPIC_ANG_RATE_FUSED,      5,  print_ang_rate_fused    },
        { TOPIC_ANG_RATE_RAW,        6,  print_ang_rate_raw      },
        /* 2. altitude */
        { TOPIC_ALT_FUSED,           7,  print_alt_fused         },
        { TOPIC_ALT_BARO,            8,  print_alt_baro          },
        { TOPIC_ALT_HOMEPOINT,       9,  print_alt_homepoint     },
        { TOPIC_HEIGHT_FUSION,      10,  print_height_fusion     },
        /* 3. position */
        { TOPIC_POS_FUSED,          11,  print_pos_fused         },
        { TOPIC_POS_VO,             12,  print_pos_vo            },
        /* 4. GPS */
        { TOPIC_GPS_DATE,           13,  print_gps_date          },
        { TOPIC_GPS_TIME,           14,  print_gps_time          },
        { TOPIC_GPS_POSITION,       15,  print_gps_position      },
        { TOPIC_GPS_VELOCITY,       16,  print_gps_velocity      },
        { TOPIC_GPS_DETAILS,        17,  print_gps_details       },
        { TOPIC_GPS_SIGNAL,         18,  print_gps_signal        },
        /* 5. RTK */
        { TOPIC_RTK_POSITION,       19,  print_rtk_position      },
        { TOPIC_RTK_VELOCITY,       20,  print_rtk_velocity      },
        { TOPIC_RTK_YAW,            21,  print_rtk_yaw           },
        { TOPIC_RTK_POSITION_INFO,  22,  print_rtk_position_info },
        { TOPIC_RTK_YAW_INFO,       23,  print_rtk_yaw_info      },
        { TOPIC_RTK_CONNECT,        24,  print_rtk_connect       },
        /* 6. RC */
        { TOPIC_RC,                 25,  print_rc                },
        { TOPIC_RC_FLAG,            26,  print_rc_flag           },
        /* 7. flight status */
        { TOPIC_STATUS_FLIGHT,      27,  print_status_flight     },
        { TOPIC_STATUS_DISPLAYMODE, 28,  print_status_displaymode},
        /* TOPIC_MOTOR_START_ERROR removed (idx 29 reserved) */
        /* 8. control device */
        { TOPIC_CONTROL_DEVICE,     29,  print_control_device    },
        /* 9. sensors / safety */
        { TOPIC_COMPASS,            30,  print_compass           },
        { TOPIC_ESC_DATA,           31,  print_esc_data          },
        { TOPIC_FLIGHT_ANOMALY,     32,  print_flight_anomaly    },
        { TOPIC_AVOID_DATA,         33,  print_avoid_data        },
        /* 10. home point */
        { TOPIC_HOME_POINT_STATUS,  34,  print_home_point_status },
        { TOPIC_HOME_POINT_INFO,    35,  print_home_point_info   },
        /* 11. battery */
        { TOPIC_BATTERY_SINGLE1,    36,  print_battery_single1   },
        { TOPIC_BATTERY_SINGLE2,    37,  print_battery_single2   },
    };
    const int n = (int)(sizeof(dispatch) / sizeof(dispatch[0]));

    for (int i = 0; i < n; i++) {
        const cJSON *sub = cJSON_GetObjectItemCaseSensitive(data, dispatch[i].name);
        if (!sub || !cJSON_IsObject(sub)) continue;
        g_counts[dispatch[i].idx]++;
        if (ds_should_print_topic(dispatch[i].idx)) {
            dispatch[i].print(sub);
        }
    }

    /* GetLatestValue response: receiving data.quaternion is treated as success */
    if (is_latest && !g_latest_done) {
        const cJSON *q = cJSON_GetObjectItemCaseSensitive(data, TOPIC_QUATERNION);
        if (q && cJSON_IsObject(q)) {
            g_latest_done = 1;
            {
                const T_ThingModelOsal *osal = ThingModelOsal_Get();
                if (osal && osal->event_signal && g_latest_event)
                    osal->event_signal(g_latest_event);
            }
            if (g_verbose) {
                LOG_INFO("[GetLatestValue response received]");
            }
        }
    }

    cJSON_Delete(root);
}

/* ============================================================
 * DataSubscription_OnPublishData placeholder callback
 *
 * This callback exists to trigger internal OSD topic subscription (CreateClient registers a session listener,
 * OnPublishData subscribes to thing/product/<sn>/osd).
 * Nested message-field decoding is not performed here;
 * the actual data parsing is done by the on_raw_message raw callback.
 * ============================================================ */
static void on_publish_data_stub(void *user, const T_DataSubscriptionPublishDataPush *p)
{
    (void)user;
    (void)p;
    /* Placeholder: nested fields are parsed by the raw message callback. */
}

/* ============================================================
 * Command-line flag parsing
 * ============================================================ */
static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng  && strcmp(argv[i], lng)  == 0) ||
            (shrt && strcmp(argv[i], shrt) == 0))
            return 1;
    return 0;
}

/* Get the value of an option that takes a value: --sn mqtt_a / -s mqtt_a. Returns def if not given (or missing value). */
static const char *flag_value(int argc, char **argv,
                               const char *lng, const char *shrt, const char *def)
{
    for (int i = 1; i < argc - 1; i++)
        if ((lng  && strcmp(argv[i], lng)  == 0) ||
            (shrt && strcmp(argv[i], shrt) == 0))
            return argv[i + 1];
    return def;
}

static void usage(const char *prog)
{
    printf(
        "Flight-controller Data Subscription interface demo\n"
        "  Subscribe FC telemetry topics / receive OSD pushes / query latest value / unsubscribe\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --sn <SN>, -s <SN>  Override the access device SN (= MQTT client_id).\n"
        "                      In multi-payload scenarios each payload needs a distinct SN to connect concurrently;\n"
        "                      equivalent to the THING_MODEL_DEVICE_SN env var, but takes higher priority.\n"
        "  --verbose, -v       Show every pushed data record (default: count only)\n"
        "  --debug [topic]     Raw capture: subscribe thing/product/<sn>/# and print received PUBLISHes.\n"
        "     without topic    Print all PUBLISHes (very verbose, good for troubleshooting connectivity)\n"
        "     with topic name  Only print messages whose data object contains that key (precise filter)\n"
        "                      e.g.: --debug battery_single_info_index1  only show battery pushes\n"
        "                            --debug quaternion    only show quaternion pushes\n"
        "  --help,    -h       Show this help and exit\n"
        "\n"
        "Run phases:\n"
        "  Phase A  dry-run (no broker): print the wire JSON of subscribe_topic / unsubscribe_topic /\n"
        "           get_latest_value plus the OSD topic string\n"
        "  Phase B  connect broker: subscribe all 38 topics -> observe pushes -> GetLatestValue ->\n"
        "           unsubscribe all topics\n"
        "\n"
        "All topics (39, in 11 groups):\n"
        "  IMU/attitude(10Hz): quaternion, acceleration_ground/body/raw, velocity,\n"
        "                  angular_rate_fused/raw\n"
        "  Altitude(1Hz):  altitude_fused/barometer/of_homepoint, height_fusion\n"
        "  Position:       position_fused(1Hz), position_vo(10Hz)\n"
        "  GPS(1Hz):       gps_date/time/position/velocity/details/signal_level\n"
        "  RTK(1Hz):       rtk_position/velocity/yaw/position_info/yaw_info/connect_status\n"
        "  RC(10Hz):       rc, rc_with_flag_data\n"
        "  Flight status(1Hz):  status_flight/displaymode\n"
        "  Control device(1Hz):  control_device\n"
        "  Sensors/safety: compass/esc_data(10Hz), flight_anomaly/avoid_data(1/10Hz)\n"
        "  Home point(1Hz):    home_point_set_status, home_point_info\n"
        "  Battery(1Hz):   battery_single_info_index1/2\n"
        "\n"
        "Environment variables (all have defaults, override as needed):\n"
        "  THING_MODEL_BROKER        broker address     (default 127.0.0.1)\n"
        "  THING_MODEL_DEVICE_SN     device SN          (default mqtt_test; overridable via --sn)\n"
        "  THING_MODEL_CA_CERT_PATH  CA certificate path\n"
        "  THING_MODEL_APP_ID / _APP_NAME / _APP_KEY / _APP_LICENSE   access authentication\n"
        "  THING_MODEL_OBSERVE_SEC   push observation window, seconds  (default 15; 0 = indefinite)\n"
        "\n"
        "Examples:\n"
        "  %s                              # default: subscribe all 38 topics, observe 15s, print stats\n"
        "  %s --verbose                    # same as above, but print each pushed record's contents\n"
        "  %s --debug                      # print all raw MQTT PUBLISHes\n"
        "  %s --debug battery_single_info_index1  # only show battery pushes (precise filter)\n"
        "  %s --debug quaternion --verbose # only show quaternion pushes + decoded contents\n"
        "  %s --sn payload_a               # connect with SN payload_a (multi-payload scenario)\n"
        "  THING_MODEL_OBSERVE_SEC=0 ./sample_things_model data_subscription"
        " --debug battery_single_info_index1  # capture battery indefinitely\n"
        "\n"
        "Device identity (global parameter, placed before the sample name to apply to all samples):\n"
        "  sample_things_model --sn <SN> data_subscription\n",
        prog, prog, prog, prog, prog, prog, prog);
}

/* ============================================================
 * Core run — shared by both ends (Linux + RTOS)
 *
 * Contract: the caller guarantees the session has connected successfully. This function only does: subscribe → observe → unsubscribe.
 * It does not Connect / Disconnect, nor open any launcher facade object.
 *
 * observe_sec: <=0 means indefinite, until sample_should_stop() is set.
 * ============================================================ */
int data_subscription_run_with_session(ThingModelClientSession_t *s,
                                       int verbose,
                                       const char *debug_filter,
                                       int observe_sec)
{
    if (!s) return -1;
    g_verbose      = verbose;
    g_debug_filter = debug_filter;

    /* event used by GetLatestValue (OSAL, shared by Linux+RTOS) */
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    if (osal && osal->event_create) g_latest_event = osal->event_create();

    /* ── 1. Create the DataSubscription client ── */
    DataSubscription_Client_t *ds = DataSubscription_CreateClient(s);
    if (!ds) {
        LOG_ERROR("[ds] DataSubscription_CreateClient failed");
        if (osal && osal->event_destroy && g_latest_event) {
            osal->event_destroy(g_latest_event);
            g_latest_event = NULL;
        }
        return -1;
    }

    /* ── 2. Register the OSD callback (triggers internal OSD topic subscription) ── */
    DataSubscription_OnPublishData(ds, on_publish_data_stub, NULL);

    /* ── 3. debug tap: subscribe to all topics under the SN ── */
    if (debug_filter && debug_filter[0]) {
        ThingModelClientSession_AddListener(s, on_raw_tap, NULL);
        const char *sn = ThingModelClientSession_GetSn(s);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", sn ? sn : "");
        ThingModelClientSession_Subscribe(s, wild, 0);
        LOG_INFO("\n[debug] raw capture on: subscribe %s (only messages containing \"%s\" are shown)",
               wild, debug_filter);
    }

    /* ── 4. raw message listener ── */
    g_raw_listener = ThingModelClientSession_AddListener(s, on_raw_message, NULL);

    /* ── 5. batch-subscribe all topics ── */
    LOG_INFO("\n[1] Subscribing all %d topics...", TOPIC_COUNT);
    int sub_ok = 0, sub_fail = 0;
    for (int i = 0; i < TOPIC_COUNT; i++) {
        if (sample_should_stop()) {
            LOG_WARN("(abort) Ctrl-C requested, subscribed %d, skipping remaining %d",
                   i, TOPIC_COUNT - i);
            break;
        }
        T_DataSubscriptionSubscribeTopicRequest req;
        DataSubscription_InitDataSubscriptionSubscribeTopicRequest(&req);
        req.topic = (char *)g_topics[i].name;
        switch (g_topics[i].freq) {
            case 50:  req.frequency = DATA_SUBSCRIPTION_FREQUENCY_50HZ; break;
            case 10:  req.frequency = DATA_SUBSCRIPTION_FREQUENCY_10HZ; break;
            case 5:   req.frequency = DATA_SUBSCRIPTION_FREQUENCY_5HZ;  break;
            default:  req.frequency = DATA_SUBSCRIPTION_FREQUENCY_1HZ;  break;
        }
        T_ThingModelCommonReply resp = {0};
        T_ThingModelReturnCode rc = DataSubscription_SubscribeTopic(ds, &req, &resp, 5000);
        if (rc == THING_MODEL_RC_TIMEOUT && !sample_should_stop()) {  /* silently retry once */
            ds_sleep_ms(500);
            memset(&resp, 0, sizeof(resp));
            rc = DataSubscription_SubscribeTopic(ds, &req, &resp, 5000);
        }
        if (rc == 0 && resp.result == 0) {
            sub_ok++;
            LOG_INFO("%-32s %2dHz  OK", g_topics[i].name, g_topics[i].freq);
        } else {
            sub_fail++;
            LOG_WARN("%-32s %2dHz  FAIL rc=%d result=%d%s%s",
                   g_topics[i].name, g_topics[i].freq, rc, resp.result,
                   resp.result_message[0] ? " msg=" : "",
                   resp.result_message[0] ? resp.result_message : "");
        }
        ds_sleep_ms(BATCH_ITEM_MS);
        if ((i + 1) % BATCH_SIZE == 0 && i + 1 < TOPIC_COUNT)
            ds_sleep_ms(BATCH_GAP_MS);
    }
    LOG_INFO("Subscription done: %d succeeded, %d failed", sub_ok, sub_fail);

    /* Zero the counters so only pushes within the observation window are counted */
    for (int i = 0; i < TOPIC_COUNT; i++) {
        g_counts[i] = 0;
        g_last_print_ms[i] = 0;
    }

    /* ── 6. observation window for pushes ── */
    if (!g_verbose)
        LOG_INFO("\n[2] Observing pushes for %ds (verbose=off, count only)...", observe_sec);
    else
        LOG_INFO("\n[2] Observing pushes for %ds ...", observe_sec);

    if (observe_sec <= 0) {
        while (!ds_should_stop()) {
            ds_sleep_ms(30000);
        }
    } else {
        int elapsed = 0;
        while (elapsed < observe_sec && !ds_should_stop()) {
            ds_sleep_ms(1000);
            elapsed++;
            if (!g_verbose && (elapsed % 5 == 0 || elapsed == observe_sec)) {
                LOG_INFO("\n    -- push stats (%ds) --", elapsed);
                for (int i = 0; i < TOPIC_COUNT; i++) {
                    LOG_INFO("%-32s: %5d msgs%s",
                           g_topics[i].name, g_counts[i],
                           g_counts[i] == 0 ? "  <- possibly unsupported" : "");
                }
            }
        }
    }

    /* ── 7. unsubscribe all topics ── */
    LOG_INFO("\n[3] Unsubscribing...");
    int unsub_ok = 0;
    for (int i = 0; i < TOPIC_COUNT; i++) {
        if (sample_should_stop()) {
            LOG_WARN("(abort) %d topics left unsubscribed, relying on disconnect cleanup", TOPIC_COUNT - i);
            break;
        }
        T_DataSubscriptionUnsubscribeTopicRequest req;
        DataSubscription_InitDataSubscriptionUnsubscribeTopicRequest(&req);
        req.topic = (char *)g_topics[i].name;
        T_ThingModelCommonReply resp = {0};
        T_ThingModelReturnCode rc = DataSubscription_UnsubscribeTopic(ds, &req, &resp, 5000);
        if (rc == THING_MODEL_RC_TIMEOUT && !sample_should_stop()) {   /* retry once on timeout */
            ds_sleep_ms(500);
            memset(&resp, 0, sizeof(resp));
            rc = DataSubscription_UnsubscribeTopic(ds, &req, &resp, 5000);
        }
        if (rc == 0) {
            unsub_ok++;
            LOG_INFO("%-32s  OK", g_topics[i].name);
        } else {
            LOG_WARN("%-32s  FAIL rc=%d result=%d%s%s",
                   g_topics[i].name, rc, resp.result,
                   resp.result_message[0] ? " msg=" : "",
                   resp.result_message[0] ? resp.result_message : "");
        }
        ds_sleep_ms(50);
    }
    LOG_INFO("Unsubscribe done: %d/%d succeeded", unsub_ok, TOPIC_COUNT);

    /* ── 8. cleanup ── */
    if (g_raw_listener >= 0) {
        ThingModelClientSession_RemoveListener(s, g_raw_listener);
        g_raw_listener = -1;
    }
    DataSubscription_DestroyClient(ds);
    if (osal && osal->event_destroy && g_latest_event) {
        osal->event_destroy(g_latest_event);
        g_latest_event = NULL;
    }
    return 0;
}

#if DS_PLATFORM_POSIX
/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback)
 *   - argv parse --sn / --verbose / --debug / --help
 *   - Phase A dry-run print of the wire format
 *   - sample_session_open + call core _run_with_session + sample_session_close
 * ============================================================ */
static int data_subscription_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (has_flag(argc, argv, "--help", "-h")) {
        usage(argv[0]);
        return 0;
    }
    int verbose = has_flag(argc, argv, "--verbose", "-v");
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);

    /* --debug [topic]: any --debug/-d turns on capture; if followed by a word not starting with "-", use it as the filter */
    const char *debug_filter = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                debug_filter = argv[i + 1];
            } else {
                debug_filter = "";   /* empty string means "print all" (downstream handles it with NULL semantics) */
            }
            break;
        }
    }

    /* ─── 1. OSAL registration + Session config ─── */
    ThingModelOsal_RegisterPosix();
    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    if (sn_override) cfg.device_sn = sn_override;
    thing_model_sample_config_print(&cfg);

    /* ─── 2. Phase A: dry-run, inspect the wire format ─── */
    {
        LOG_INFO("=== Phase A: inspect wire format first (no publish) ===");
        T_DataSubscriptionSubscribeTopicRequest sub_req;
        DataSubscription_InitDataSubscriptionSubscribeTopicRequest(&sub_req);
        sub_req.topic     = TOPIC_QUATERNION;
        sub_req.frequency = DATA_SUBSCRIPTION_FREQUENCY_10HZ;
        char *sub_payload = DataSubscription_BuildSubscribeTopicPayload(
            THING_MODEL_TOPIC_STYLE_V2, 1, &sub_req);
        LOG_INFO("subscribe_topic(quaternion,10Hz) payload = %s", sub_payload);
        free(sub_payload);

        T_DataSubscriptionUnsubscribeTopicRequest unsub_req;
        DataSubscription_InitDataSubscriptionUnsubscribeTopicRequest(&unsub_req);
        unsub_req.topic = TOPIC_QUATERNION;
        char *unsub_payload = DataSubscription_BuildUnsubscribeTopicPayload(
            THING_MODEL_TOPIC_STYLE_V2, 2, &unsub_req);
        LOG_INFO("unsubscribe_topic(quaternion)   payload = %s", unsub_payload);
        free(unsub_payload);

        T_DataSubscriptionGetLatestValueRequest latest_req;
        DataSubscription_InitDataSubscriptionGetLatestValueRequest(&latest_req);
        latest_req.topic = TOPIC_QUATERNION;
        char *latest_payload = DataSubscription_BuildGetLatestValuePayload(
            THING_MODEL_TOPIC_STYLE_V2, 3, &latest_req);
        LOG_INFO("get_latest_value(quaternion)    payload = %s", latest_payload);
        free(latest_payload);

        char *osd_topic = DataSubscription_BuildTopic(
            THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_OSD);
        LOG_INFO("OSD topic                       = %s", osd_topic);
        free(osd_topic);

        char *svc_topic = DataSubscription_BuildTopic(
            THING_MODEL_TOPIC_STYLE_V2, cfg.device_sn, THING_MODEL_TOPIC_SERVICE);
        LOG_INFO("services topic                  = %s", svc_topic);
        free(svc_topic);
    }

    /* ─── 3. Phase B: connect to broker, call core run ─── */
    LOG_INFO("\n=== Phase B: connect broker, call real interfaces ===");
    sample_aircraft_info_set_enabled(false);   /* avoid the OSD channel flooding the screen */
    bool owned = false;
    ThingModelClientSession_t *s = sample_session_acquire(ctx, &cfg, &owned);
    if (!s) {
        LOG_ERROR("session connect failed (broker=%s)", cfg.broker_address);
        LOG_ERROR("hint: Phase A can run without a broker to view the JSON");
        return 1;
    }

    const int observe_sec = atoi(thing_model_sample_getenv("THING_MODEL_OBSERVE_SEC", "15"));
    int rc = data_subscription_run_with_session(s, verbose, debug_filter, observe_sec);
    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER(data_subscription,
    "FC data subscription: subscribe all 38 topics / push / query latest value / unsubscribe",
    data_subscription_run);
#endif /* DS_PLATFORM_POSIX */
