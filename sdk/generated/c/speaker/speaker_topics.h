/**
 * @file    speaker_topics.h
 * @brief   Speaker MQTT topic segment / method / property name constants.
 */
#ifndef SPEAKER_TOPICS_H
#define SPEAKER_TOPICS_H

#define SPEAKER_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define SPEAKER_TOPIC_OWNER     "uav"
#define SPEAKER_TOPIC_CAP_NAME  "speaker"

/* ---- service method names (payload.method) ---- */
#define SPEAKER_METHOD_SET_WORK_MODE  "set_work_mode"
#define SPEAKER_METHOD_SET_PLAY_MODE  "set_play_mode"
#define SPEAKER_METHOD_SET_VOLUME  "set_volume"
#define SPEAKER_METHOD_START_PLAY  "start_play"
#define SPEAKER_METHOD_STOP_PLAY  "stop_play"
#define SPEAKER_METHOD_TRANS_START  "trans_start"
#define SPEAKER_METHOD_TRANS_DATA  "trans_data"
#define SPEAKER_METHOD_TRANS_FINISH  "trans_finish"

/* ---- push method names (event/osd/state payload.method) ---- */
#define SPEAKER_METHOD_STATE  "state"


#ifdef __cplusplus
}
#endif
#endif /* SPEAKER_TOPICS_H */
