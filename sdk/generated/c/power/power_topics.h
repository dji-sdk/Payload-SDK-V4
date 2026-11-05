/**
 * @file    power_topics.h
 * @brief   Power MQTT topic segment / method / property name constants.
 */
#ifndef POWER_TOPICS_H
#define POWER_TOPICS_H

#define POWER_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define POWER_TOPIC_OWNER     "uav"
#define POWER_TOPIC_CAP_NAME  "power"

/* ---- service method names (payload.method) ---- */
#define POWER_METHOD_APPLY_HIGH_POWER  "apply_high_power"
#define POWER_METHOD_POWER_OFF_NOTIFICATION  "off_notification"



#ifdef __cplusplus
}
#endif
#endif /* POWER_TOPICS_H */
