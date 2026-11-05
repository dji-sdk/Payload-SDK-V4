/**
 * @file    time_sync_topics.h
 * @brief   TimeSync MQTT topic segment / method / property name constants.
 */
#ifndef TIME_SYNC_TOPICS_H
#define TIME_SYNC_TOPICS_H

#define TIME_SYNC_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define TIME_SYNC_TOPIC_OWNER     "uav"
#define TIME_SYNC_TOPIC_CAP_NAME  "time"

/* ---- service method names (payload.method) ---- */
#define TIME_SYNC_METHOD_SET_SIGNAL_MODE  "sync_set_signal_mode"

/* ---- push method names (event/osd/state payload.method) ---- */
#define TIME_SYNC_METHOD_SYNC  "sync"


#ifdef __cplusplus
}
#endif
#endif /* TIME_SYNC_TOPICS_H */
