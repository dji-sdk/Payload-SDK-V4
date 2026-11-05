/**
 * @file    data_subscription_topics.h
 * @brief   DataSubscription MQTT topic segment / method / property name constants.
 */
#ifndef DATA_SUBSCRIPTION_TOPICS_H
#define DATA_SUBSCRIPTION_TOPICS_H

#define DATA_SUBSCRIPTION_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define DATA_SUBSCRIPTION_TOPIC_OWNER     "uav"
#define DATA_SUBSCRIPTION_TOPIC_CAP_NAME  "fc"

/* ---- service method names (payload.method) ---- */
#define DATA_SUBSCRIPTION_METHOD_SUBSCRIBE_TOPIC  "subscribe_topic"
#define DATA_SUBSCRIPTION_METHOD_UNSUBSCRIBE_TOPIC  "unsubscribe_topic"
#define DATA_SUBSCRIPTION_METHOD_GET_LATEST_VALUE  "get_latest_value"

/* ---- push method names (event/osd/state payload.method) ---- */
#define DATA_SUBSCRIPTION_METHOD_PUBLISH_DATA  "publish_data"


#ifdef __cplusplus
}
#endif
#endif /* DATA_SUBSCRIPTION_TOPICS_H */
