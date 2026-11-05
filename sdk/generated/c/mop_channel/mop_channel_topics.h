/**
 * @file    mop_channel_topics.h
 * @brief   MopChannel MQTT topic segment / method / property name constants.
 */
#ifndef MOP_CHANNEL_TOPICS_H
#define MOP_CHANNEL_TOPICS_H

#define MOP_CHANNEL_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define MOP_CHANNEL_TOPIC_OWNER     "uav"
#define MOP_CHANNEL_TOPIC_CAP_NAME  "mop_channel"

/* ---- service method names (payload.method) ---- */
#define MOP_CHANNEL_METHOD_ENABLE  "enable"
#define MOP_CHANNEL_METHOD_DISABLE  "disable"

/* ---- push method names (event/osd/state payload.method) ---- */
#define MOP_CHANNEL_METHOD_CHANNEL_EVENT  "event"


#ifdef __cplusplus
}
#endif
#endif /* MOP_CHANNEL_TOPICS_H */
