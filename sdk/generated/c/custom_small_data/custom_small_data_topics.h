/**
 * @file    custom_small_data_topics.h
 * @brief   CustomSmallData MQTT topic segment / method / property name constants.
 */
#ifndef CUSTOM_SMALL_DATA_TOPICS_H
#define CUSTOM_SMALL_DATA_TOPICS_H

#define CUSTOM_SMALL_DATA_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define CUSTOM_SMALL_DATA_TOPIC_OWNER     "uav"
#define CUSTOM_SMALL_DATA_TOPIC_CAP_NAME  "custom_small_data"


/* ---- push method names (event/osd/state payload.method) ---- */
#define CUSTOM_SMALL_DATA_METHOD_TRANSMIT  "transmit_req"
#define CUSTOM_SMALL_DATA_METHOD_TRANSMIT_PUSH  "transmit_event"


#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_SMALL_DATA_TOPICS_H */
