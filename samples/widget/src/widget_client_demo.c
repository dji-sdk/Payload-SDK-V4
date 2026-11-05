/**
 * @file    widget_client_demo.c
 * @brief   widget + speaker + device info combined demo
 *          (Linux POSIX + RTOS FreeRTOS share a single source).
 *
 * Reimplements the legacy C++ widget demo
 * (widget_test.cpp = widget + speaker(device side) + device_info + HTTP file server).
 *
 * Business logic is split into modules:
 *   app_widget.{c,h}       widget: dynamically reads config to build the table + 3 service handlers + rich-text float window assets
 *   app_speaker.{c,h}      speaker: 8 handlers + audio hex persistence to disk + md5 verification
 *   http_file_server.{c,h} static-file HTTP server (mongoose; --no-http disables it; POSIX only)
 *   widget_md5.{c,h}       built-in MD5 (zero external dependencies)
 * This file only does: CLI parsing, session connection, handler registration, push thread, cleanup.
 *
 * Platform split:
 *   Linux (POSIX): argv/getenv parsing --sn/--config/--audio/--http/--no-http/--observe,
 *                  starts HTTP server + Phase A dry-run
 *   RTOS:          no argv/env, core logic shared by both ends via widget_run_with_session()
 *   sleep/usleep → OSAL sleep_ms, time/localtime → OSAL time_ms
 *   http_file_server.c is entirely POSIX-specific, not compiled on the RTOS side
 *   app_widget.c file enumeration (opendir/readdir/stat) + app_speaker.c file operations (fopen/mkdir) are both wrapped with #if POSIX
 */
#include "thing_model_client/thing_model_client_session.h"
#include "thing_model_client/thing_model_client_osal.h"

#include "widget/widget_client.h"
#include "widget/widget_publisher.h"
#include "widget/widget_json.h"
#include "widget/widget_topics.h"
#include "speaker/speaker_client.h"
#include "speaker/speaker_publisher.h"
#include "speaker/speaker_json.h"
#include "speaker/speaker_topics.h"
#include "common/common_types.h"

#include "app_widget.h"
#include "app_speaker.h"
#include "http_file_server.h"

#include "thing_model_client_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Platform split
 * ============================================================ */
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define WIDGET_PLATFORM_POSIX 1
#  include <unistd.h>
#  include <time.h>
#  include "sample_config.h"
#  include "sample_register.h"
#  include "sample_lifecycle.h"
#  include "sample_session.h"
#else
#  define WIDGET_PLATFORM_POSIX 0
#  include "FreeRTOS.h"
#  include "task.h"
#  include "sample_lifecycle.h"
#endif

/* ---- global config (set by argv on Linux, defaults on RTOS) ---- */
static const char *g_config_root = "./config";
static const char *g_audio_dir   = "./audio";
static const char *g_http_url    = "http://0.0.0.0:8000";
static int         g_no_http      = 0;

/* ---- platform-agnostic sleep convenience wrapper ---- */
static void widget_sleep_ms(uint32_t ms) { ThingModelOsal_SleepMs(ms); }

/* ---- raw packet tap --------------------------------------------------- */
static void on_raw_tap(void *user, const char *topic, const char *payload, size_t len)
{
    (void)user;
    LOG_INFO("[RAW] %s | %.*s", topic, (int)len, payload);
}

#if WIDGET_PLATFORM_POSIX
/* ---- HTTP file server language resolution: returns the language directory name selected by get_file_list. ---- */
static const char *http_lang_resolver(void *user)
{
    return ((const AppWidget *)user)->current_lang;
}
#endif

/* ---- rich-text float window: time + speaker state + widget last action ---- */
static void build_float_msg(const AppWidget *w, const AppSpeaker *sp, char *out, size_t cap)
{
    char ts[32] = "";
#if WIDGET_PLATFORM_POSIX
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
#else
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    uint64_t ms = (osal && osal->time_ms) ? osal->time_ms() : 0;
    snprintf(ts, sizeof(ts), "t=%llus", (unsigned long long)(ms / 1000));
#endif
    snprintf(out, cap,
             "%s\nstate:%s mode:%s play:%s vol:%d\n[%s] %s",
             ts,
             app_speaker_state_str(sp->system_state),
             app_speaker_work_mode_str(sp->work_mode),
             app_speaker_play_mode_str(sp->play_mode),
             sp->play_volume,
             w->last_action_time, w->last_action);
}

/* ============================================================
 * Core run — shared by both ends (Linux + RTOS)
 *
 * The caller guarantees the session has Connected successfully. This function only handles: init app →
 * (POSIX) start HTTP server → register handlers → 50ms push loop → teardown.
 * Does not Connect / Disconnect.
 * ============================================================ */
int widget_run_with_session(ThingModelClientSession_t *session, int observe_sec)
{
    if (!session) return -1;
    const char *sn = ThingModelClientSession_GetSn(session);

    /* business module initialization */
    AppWidget      w;   app_widget_init(&w, g_config_root);
    AppSpeaker     sp;  app_speaker_init(&sp, g_audio_dir);

    char cfg_json[400];
    snprintf(cfg_json, sizeof(cfg_json), "%s/en/widget_config.json", g_config_root);
    app_widget_load_config(&w, cfg_json);

#if WIDGET_PLATFORM_POSIX
    /* HTTP file server (mounts the config root, serves config/<lang> by the language selected via get_file_list). */
    if (!g_no_http) {
        if (http_file_server_start(g_http_url, g_config_root, http_lang_resolver, &w) == 0)
            LOG_INFO("[http] serving %s/<lang> on %s (lang follows get_file_list)",
                   g_config_root, g_http_url);
        else
            LOG_WARN("[http] start FAIL on %s (--no-http to disable)", g_http_url);
    }
#endif

    widget_sleep_ms(2000);

    /* register widget(3) + speaker(8) handlers */
    WidgetImpl_t  *wimpl  = WidgetImpl_Create(session, sn);
    WidgetImpl_OnGetFileList(wimpl, app_widget_on_get_file_list, &w);
    WidgetImpl_OnSetValue   (wimpl, app_widget_on_set_value,    &w);
    WidgetImpl_OnGetValue   (wimpl, app_widget_on_get_value,    &w);

    SpeakerImpl_t *spimpl = SpeakerImpl_Create(session, sn);
    SpeakerImpl_OnSetWorkMode(spimpl, app_speaker_on_set_work_mode, &sp);
    SpeakerImpl_OnSetPlayMode(spimpl, app_speaker_on_set_play_mode, &sp);
    SpeakerImpl_OnSetVolume  (spimpl, app_speaker_on_set_volume,    &sp);
    SpeakerImpl_OnStartPlay  (spimpl, app_speaker_on_start_play,    &sp);
    SpeakerImpl_OnStopPlay   (spimpl, app_speaker_on_stop_play,     &sp);
    SpeakerImpl_OnTransStart (spimpl, app_speaker_on_trans_start,   &sp);
    SpeakerImpl_OnTransData  (spimpl, app_speaker_on_trans_data,    &sp);
    SpeakerImpl_OnTransFinish(spimpl, app_speaker_on_trans_finish,  &sp);
    LOG_INFO("[1] widget+speaker handler registered");

    /* publishers */
    Widget_Publisher_t     *wpub  = Widget_CreatePublisher(session);
    Speaker_Publisher_t    *sppub = Speaker_CreatePublisher(session);

    /* Push loop: 50ms base tick (20 slots/sec), the three streams staggered in turn, at most 1 on state at any instant:
     *   speaker_state : phase   0ms, period 200ms(5Hz) → slot%4==0  (0,4,8,12,16)
     *   widget_state  : phase  50ms, period 1000ms(1Hz) → slot==1
     *   float_window  : phase 150ms, period 1000ms(1Hz) → slot==3
     * All three go through the state topic, each with a distinct wire method (the peer demultiplexes by payload.method), never at the same instant. */
    LOG_INFO("[2] push thread started (speaker 5Hz, widget/float 1Hz); window %ds (0=indefinite)...",
           observe_sec);
    long max_ticks = (observe_sec <= 0) ? -1 : (long)observe_sec * 20;
    long n = 0;
    int  slot = 0;   /* 0..19, 50ms per slot */
    while ((max_ticks < 0 || n < max_ticks) && !sample_should_stop()) {
        if ((slot & 3) == 0) {
            /* speaker_state 5Hz —— phase 0ms */
            T_SpeakerStatePush sps;
            app_speaker_fill_state(&sp, &sps);
            Speaker_PublishState(sppub, &sps);
        } else if (slot == 1) {
            /* widget_state 1Hz —— phase 50ms */
            T_WidgetStateItem items[APP_WIDGET_MAX];
            T_WidgetStatePush wsp;
            app_widget_fill_state(&w, &wsp, items);
            Widget_PublishState(wpub, &wsp);
        } else if (slot == 3) {
            /* float_window rich text 1Hz —— phase 150ms */
            char fmsg[600];
            build_float_msg(&w, &sp, fmsg, sizeof(fmsg));
            T_WidgetFloatWindowPush fw = { .message = fmsg };
            Widget_PublishFloatWindowMsg(wpub, &fw);
        }
        slot = (slot + 1) % 20;
        n++;
        if (n % 600 == 0)   /* report received widget service calls once every 30s */
            LOG_INFO("[widget] tick=%ld file=%d set=%d get=%d",
                   n, w.n_get_file_list, w.n_set_value, w.n_get_value);
        widget_sleep_ms(50);
    }

    /* cleanup */
#if WIDGET_PLATFORM_POSIX
    if (http_file_server_is_running()) http_file_server_stop();
#endif
    Speaker_DestroyPublisher(sppub);
    Widget_DestroyPublisher(wpub);
    SpeakerImpl_Destroy(spimpl);
    WidgetImpl_Destroy(wimpl);
    return 0;
}

/* ============================================================
 * Linux launcher entry (SAMPLE_REGISTER callback)
 * ============================================================ */
#if WIDGET_PLATFORM_POSIX

/* ---- CLI helpers ------------------------------------------------------ */
static int has_flag(int argc, char **argv, const char *lng, const char *shrt)
{
    for (int i = 1; i < argc; i++)
        if ((lng && !strcmp(argv[i], lng)) || (shrt && !strcmp(argv[i], shrt))) return 1;
    return 0;
}
static const char *flag_value(int argc, char **argv, const char *lng, const char *shrt, const char *def)
{
    for (int i = 1; i < argc - 1; i++)
        if ((lng && !strcmp(argv[i], lng)) || (shrt && !strcmp(argv[i], shrt))) return argv[i + 1];
    return def;
}

static void usage(const char *prog)
{
    printf(
        "Widget + speaker + device info combined demo (device-side impl + push).\n\n"
        "Usage: %s [options]\n\n"
        "Options:\n"
        "  --sn <SN>,-s      access SN (= MQTT client_id)\n"
        "  --config <dir>    widget config root (default ./config, with en/ zh/ under it)\n"
        "  --audio <dir>     speaker audio persist directory (default ./audio)\n"
        "  --http <url>      HTTP file server listen address (default http://0.0.0.0:8000)\n"
        "  --no-http         do not start the HTTP file server\n"
        "  --observe <sec>   push/receive window seconds (default 0=indefinite, no auto-exit; >0 exits when reached)\n"
        "  --debug, -d       raw capture: subscribe thing/product/<sn>/# and print every PUBLISH\n"
        "  --help,  -h       this help\n\n"
        "Phases:\n"
        "  Phase A  dry-run (no broker connection): print representative wire / topic\n"
        "  Phase B  connect broker: register widget(3)+speaker(8) handlers, start push thread\n",
        prog);
}

/* ---- Phase A: representative wire / topic (no broker connection) ---- */
static void phase_a(const char *sn)
{
    LOG_INFO("=== Phase A: representative wire / topic (no publish) ===");

    T_WidgetSetValueRequest sv = { .widget_index = 2, .widget_type = 2, .widget_value = 1 };
    char *p = Widget_BuildSetValuePayload(THING_MODEL_TOPIC_STYLE_V2, 2, &sv);
    LOG_INFO("[widget] set_value req   = %s", p); free(p);

    T_SpeakerSetVolumeRequest vol = { .volume = 80 };
    p = Speaker_BuildSetVolumePayload(THING_MODEL_TOPIC_STYLE_V2, 9, &vol);
    LOG_INFO("[speaker] set_volume req = %s", p); free(p);

    /* speaker_trans_data: bytes audio → wire hex */
    uint8_t audio[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    T_SpeakerTransDataRequest td = { .packet_index = 0, .offset = 0,
                                     .data = { .data = audio, .len = sizeof(audio) } };
    p = Speaker_BuildTransDataPayload(THING_MODEL_TOPIC_STYLE_V2, 10, &td);
    LOG_INFO("[speaker] trans_data req = %s", p); free(p);

    /* speaker_state push */
    T_SpeakerStatePush ss = { .system_state = 0, .work_mode = 1, .play_mode = 0,
                              .play_volume = 50, .play_file_name = (char *)"",
                              .play_file_uuid = (char *)"" };
    char *data = SpeakerJson_EncodeSpeakerStatePush(&ss);
    char *wire = SpeakerJson_EncodeServiceRequest(100, SPEAKER_TOPIC_CAP_NAME "_" SPEAKER_METHOD_STATE, data);
    LOG_INFO("[speaker] state push     = %s", wire); free(wire); free(data);

    char *t = Widget_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, sn, THING_MODEL_TOPIC_SERVICE);
    LOG_INFO("topic services       = %s", t); free(t);
    t = Widget_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, sn, THING_MODEL_TOPIC_OSD);
    LOG_INFO("topic osd(state push)= %s", t); free(t);
    t = Widget_BuildTopic(THING_MODEL_TOPIC_STYLE_V2, sn, THING_MODEL_TOPIC_STATE);
    LOG_INFO("topic state          = %s", t); free(t);
}

static int widget_run(const SampleCtx *ctx)
{
    int    argc = ctx->argc;
    char **argv = ctx->argv;

    if (has_flag(argc, argv, "--help", "-h")) { usage(argv[0]); return 0; }

    const int   debug_tap   = has_flag(argc, argv, "--debug", "-d");
    const char *sn_override = flag_value(argc, argv, "--sn", "-s", NULL);

    /* global config set by argv */
    g_config_root = flag_value(argc, argv, "--config", NULL, "./config");
    g_audio_dir   = flag_value(argc, argv, "--audio", NULL, "./audio");
    g_http_url    = flag_value(argc, argv, "--http", NULL, "http://0.0.0.0:8000");
    g_no_http     = has_flag(argc, argv, "--no-http", NULL);
    const int observe_sec = atoi(flag_value(argc, argv, "--observe", NULL, "0"));

    /* OSAL + session config */
    ThingModelOsal_RegisterPosix();
    ThingModelClientSession_Config_t cfg = *ctx->cfg;
    if (sn_override) cfg.device_sn = sn_override;
    if (!ctx->background) thing_model_sample_config_print(&cfg);  /* self-check banner: skip for resident services (noise/dupe on the shared session) */

    /* Phase A dry-run (skipped for a resident background service — pure log noise there). */
    if (!ctx->background) phase_a(cfg.device_sn);

    /* Phase B */
    LOG_INFO("\n=== Phase B: connect broker ===");
    bool owned = false;
    ThingModelClientSession_t *s = sample_session_acquire(ctx, &cfg, &owned);
    if (!s) {
        LOG_ERROR("session connect failed (broker=%s); Phase A can validate wire on its own",
                cfg.broker_address);
        return 1;
    }

    if (debug_tap) {
        ThingModelClientSession_AddListener(s, on_raw_tap, NULL);
        char wild[160];
        snprintf(wild, sizeof(wild), "thing/product/%s/#", cfg.device_sn);
        ThingModelClientSession_Subscribe(s, wild, 0);
        LOG_INFO("[debug] capture subscribe %s", wild);
    }

    int rc = widget_run_with_session(s, observe_sec);

    sample_session_release(s, owned);
    return rc;
}

SAMPLE_REGISTER_SERVICE(widget, "widget+speaker+device info combined demo (impl + multi-stream push + HTTP)", widget_run);

#endif /* WIDGET_PLATFORM_POSIX */
