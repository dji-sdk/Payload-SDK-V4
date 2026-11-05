/**
 * @file    device_info_topics.h
 * @brief   DeviceInfo MQTT topic segment / method / property name constants.
 */
#ifndef DEVICE_INFO_TOPICS_H
#define DEVICE_INFO_TOPICS_H

#define DEVICE_INFO_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define DEVICE_INFO_TOPIC_OWNER     "uav"
#define DEVICE_INFO_TOPIC_CAP_NAME  "device"


/* ---- push method names (event/osd/state payload.method) ---- */
#define DEVICE_INFO_METHOD_INFO  "info"


#ifdef __cplusplus
}
#endif
#endif /* DEVICE_INFO_TOPICS_H */
