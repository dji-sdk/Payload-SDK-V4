/*
 * app_speaker.h — speaker business-logic module (decoupled from transport/platform for MCU reuse).
 *
 * Holds the speaker state + audio file receive assembly; provides 8 SpeakerImpl handlers and the
 * state push snapshot. Persisting audio to disk uses POSIX (stdio), concentrated in this file.
 */
#ifndef APP_SPEAKER_H
#define APP_SPEAKER_H

#include "speaker/speaker_impl.h"
#include "speaker/speaker_types.h"
#include "common/common_types.h"   /* T_Placeholder */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int  system_state;             /* 0=idle 1=transmitting 2=playing 3=error 4=tts */
    int  work_mode;                /* 0=tts 1=voice */
    int  play_mode;                /* 0=single 1=loop */
    int  play_volume;              /* 0..100 */
    char play_file_name[128];
    char play_file_uuid[64];

    char audio_dir[256];           /* audio persistence directory, e.g. "./audio" */

    /* current audio transfer session */
    struct {
        int   active;
        char  uuid[64];
        char  file_name[160];      /* name after appending the .tts/.voice suffix */
        FILE *fp;
        char  tmp_path[320];
        int   total_bytes;
        int   recv_bytes;          /* bytes written so far this session (for progress display) */
        int   packet_count;        /* number of data packets received (for progress display) */
        long  last_report_ms;      /* timestamp (ms) of the last progress print; print at most once per 200ms */
        int   fail_count;          /* md5 verification failure count; incremented each finish retry round from the peer, giving up past the threshold */
    } xfer;
} AppSpeaker;

/** Initialize state (default voice / single / vol 50) + record the audio directory. */
void app_speaker_init(AppSpeaker *app, const char *audio_dir);

/** Fill the state push snapshot. */
void app_speaker_fill_state(const AppSpeaker *app, T_SpeakerStatePush *push);

/* State strings (for the float window / printing). */
const char *app_speaker_state_str(int s);
const char *app_speaker_work_mode_str(int m);
const char *app_speaker_play_mode_str(int m);

/* ---- SpeakerImpl handlers (user passes AppSpeaker* at registration) ---- */
T_ThingModelReturnCode app_speaker_on_set_work_mode(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetWorkModeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_set_play_mode(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetPlayModeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_set_volume(SpeakerImpl_t *impl, void *user,
        const T_SpeakerSetVolumeRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_start_play(SpeakerImpl_t *impl, void *user,
        const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_stop_play(SpeakerImpl_t *impl, void *user,
        const T_Placeholder *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_trans_start(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransStartRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_trans_data(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransDataRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_speaker_on_trans_finish(SpeakerImpl_t *impl, void *user,
        const T_SpeakerTransFinishRequest *req, T_ThingModelCommonReply *reply);

#ifdef __cplusplus
}
#endif

#endif /* APP_SPEAKER_H */
