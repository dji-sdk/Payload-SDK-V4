/*
 * app_speaker.c — speaker business-logic implementation.
 *
 * Note: req->data for trans_data has already been decoded by the runtime from the wire hex into binary
 * (T_ThingModelBytes {data,len}); the handler can just write it to file by offset.
 */
#include "app_speaker.h"
#include "widget_md5.h"

#include "thing_model_client/thing_model_client_osal.h"

#include "thing_model_client_log.h"

#include <string.h>
#include <stdlib.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define APP_SPEAKER_PLATFORM_POSIX 1
#  include <sys/stat.h>
#else
#  define APP_SPEAKER_PLATFORM_POSIX 0
#endif

/* ---- state strings ------------------------------------------------------- */

const char *app_speaker_state_str(int s)
{
    switch (s) {
        case 0: return "idle";
        case 1: return "transmitting";
        case 2: return "playing";
        case 3: return "error";
        case 4: return "tts_conversion";
        default: return "unknown";
    }
}
const char *app_speaker_work_mode_str(int m)
{
    return (m == 0) ? "tts" : (m == 1) ? "voice" : "unknown";
}
const char *app_speaker_play_mode_str(int m)
{
    return (m == 0) ? "single" : (m == 1) ? "loop" : "unknown";
}

/* ---- init / snapshot ------------------------------------------------------ */

void app_speaker_init(AppSpeaker *app, const char *audio_dir)
{
    memset(app, 0, sizeof(*app));
    app->system_state = 0;   /* idle */
    app->work_mode    = 1;   /* voice */
    app->play_mode    = 0;   /* single */
    app->play_volume  = 50;
    snprintf(app->audio_dir, sizeof(app->audio_dir), "%s", audio_dir ? audio_dir : "./audio");
}

void app_speaker_fill_state(const AppSpeaker *app, T_SpeakerStatePush *push)
{
    push->system_state   = app->system_state;
    push->work_mode      = app->work_mode;
    push->play_mode      = app->play_mode;
    push->play_volume    = app->play_volume;
    push->play_file_name = (char *)app->play_file_name;
    push->play_file_uuid = (char *)app->play_file_uuid;
}

static void xfer_cleanup(AppSpeaker *app)
{
#if APP_SPEAKER_PLATFORM_POSIX
    if (app->xfer.fp) { fclose(app->xfer.fp); app->xfer.fp = NULL; }
    if (app->xfer.tmp_path[0]) remove(app->xfer.tmp_path);
#else
    if (app->xfer.fp) { app->xfer.fp = NULL; }
#endif
    memset(&app->xfer, 0, sizeof(app->xfer));
}

/* ---- setter handlers --------------------------------------------------- */

T_ThingModelReturnCode app_speaker_on_set_work_mode(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetWorkModeRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;
    app->work_mode = req->work_mode;
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] set_work_mode %d (%s)",
           req->work_mode, app_speaker_work_mode_str(req->work_mode));
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_set_play_mode(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetPlayModeRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;
    app->play_mode = req->play_mode;
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] set_play_mode %d (%s)",
           req->play_mode, app_speaker_play_mode_str(req->play_mode));
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_set_volume(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetVolumeRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;
    if (req->volume < 0 || req->volume > 100) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "volume out of range");
        LOG_WARN("[speaker] set_volume %d FAIL (range)", req->volume);
        return THING_MODEL_RC_OK;
    }
    app->play_volume = req->volume;
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] set_volume %d OK", req->volume);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_start_play(SpeakerImpl_t *impl, void *user,
        const T_Placeholder *req, T_ThingModelCommonReply *reply)
{
    (void)impl; (void)req;
    AppSpeaker *app = user;
    app->system_state = 2;   /* playing */
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] start_play OK (playing)");
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_stop_play(SpeakerImpl_t *impl, void *user,
        const T_Placeholder *req, T_ThingModelCommonReply *reply)
{
    (void)impl; (void)req;
    AppSpeaker *app = user;
    app->system_state = 0;   /* idle */
    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] stop_play OK (idle)");
    return THING_MODEL_RC_OK;
}

/* ---- audio transfer handlers ------------------------------------------ */

T_ThingModelReturnCode app_speaker_on_trans_start(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransStartRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;

    if (app->xfer.active) xfer_cleanup(app);
#if APP_SPEAKER_PLATFORM_POSIX
    mkdir(app->audio_dir, 0755);
#endif
    memset(&app->xfer, 0, sizeof(app->xfer));

    const char *uuid = req->uuid ? req->uuid : "";
    const char *fname = req->file_name ? req->file_name : "audio";
    snprintf(app->xfer.uuid, sizeof(app->xfer.uuid), "%s", uuid);

    /* Append the file-name suffix: work_mode 0=tts → .tts, otherwise .voice. */
    char base[128];
    snprintf(base, sizeof(base), "%s", fname);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    const char *suffix = (app->work_mode == 0) ? ".tts" : ".voice";
    snprintf(app->xfer.file_name, sizeof(app->xfer.file_name), "%s%s", base, suffix);
    snprintf(app->xfer.tmp_path, sizeof(app->xfer.tmp_path), "%s/%s.tmp", app->audio_dir, uuid);

#if APP_SPEAKER_PLATFORM_POSIX
    app->xfer.fp = fopen(app->xfer.tmp_path, "wb");
    if (!app->xfer.fp) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "failed to create file");
        LOG_ERROR("[speaker] trans_start create %s FAIL", app->xfer.tmp_path);
        return THING_MODEL_RC_OK;
    }
#else
    app->xfer.fp = (void *)1;   /* non-NULL sentinel: transfer active (no real file on RTOS) */
#endif

    app->xfer.active = 1;
    app->system_state = 1;   /* transmitting */
    snprintf(app->play_file_name, sizeof(app->play_file_name), "%s", fname);
    snprintf(app->play_file_uuid, sizeof(app->play_file_uuid), "%s", uuid);

    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] trans_start file=%s uuid=%s type=%d br=%d OK",
           app->xfer.file_name, uuid, req->file_type, req->decode_bitrate);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_trans_data(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransDataRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;

    if (!app->xfer.active || !app->xfer.fp) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "no active transfer");
        LOG_ERROR("[speaker] trans_data no active transfer FAIL");
        return THING_MODEL_RC_OK;
    }

    /* req->data is already binary (decoded by the runtime from the wire hex). */
    if (req->data.data && req->data.len > 0) {
#if APP_SPEAKER_PLATFORM_POSIX
        fseek(app->xfer.fp, req->offset, SEEK_SET);
        size_t w = fwrite(req->data.data, 1, req->data.len, app->xfer.fp);
        if (w != req->data.len)
            LOG_WARN("[speaker] trans_data write mismatch exp=%u got=%u",
                   (unsigned)req->data.len, (unsigned)w);
        fseek(app->xfer.fp, 0, SEEK_END);
        app->xfer.total_bytes = (int)ftell(app->xfer.fp);
#else
        app->xfer.total_bytes += (int)req->data.len;
#endif
        app->xfer.recv_bytes += (int)req->data.len;
        app->xfer.packet_count++;
    }

    /* Progress print: let the background see that audio is being transferred. Print immediately on the first
     * packet, then at most once per 200ms afterwards, to avoid high-frequency data packets flooding the log. */
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    long now_ms = (osal && osal->time_ms) ? (long)osal->time_ms() : 0;
    if (app->xfer.packet_count == 1 ||
        now_ms == 0 || now_ms - app->xfer.last_report_ms >= 200) {
        LOG_INFO("[speaker] trans_data recv pkt#%d off=%u len=%u total=%d bytes",
               app->xfer.packet_count, (unsigned)req->offset,
               (unsigned)req->data.len, app->xfer.total_bytes);
        app->xfer.last_report_ms = now_ms;
    }

    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_speaker_on_trans_finish(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransFinishRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppSpeaker *app = user;

    if (!app->xfer.active || !app->xfer.fp) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "no active transfer");
        LOG_ERROR("[speaker] trans_finish no active transfer FAIL");
        return THING_MODEL_RC_OK;
    }

#if APP_SPEAKER_PLATFORM_POSIX
    fclose(app->xfer.fp);
    app->xfer.fp = NULL;

    char got[33] = "";
    widget_md5_file(app->xfer.tmp_path, got);

    const char *want = (req->md5 ? req->md5 : "");
    if (want[0] && strcasecmp(got, want) != 0) {
        remove(app->xfer.tmp_path);
        memset(&app->xfer, 0, sizeof(app->xfer));
        app->system_state = 0;
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "md5 mismatch");
        LOG_ERROR("[speaker] trans_finish md5 want=%s got=%s FAIL", want, got);
        return THING_MODEL_RC_OK;
    }

    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s/%s", app->audio_dir, app->xfer.file_name);
    rename(app->xfer.tmp_path, final_path);
#else
    app->xfer.fp = NULL;
    char got[33] = "";
#endif

    int total = app->xfer.total_bytes;
    char saved_name[160];
    snprintf(saved_name, sizeof(saved_name), "%s", app->xfer.file_name);
    memset(&app->xfer, 0, sizeof(app->xfer));
    app->system_state = 0;

    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    LOG_INFO("[speaker] trans_finish file=%s size=%d md5=%s OK",
           saved_name, total, got);
    return THING_MODEL_RC_OK;
}
