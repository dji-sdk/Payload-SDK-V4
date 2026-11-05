/**
 * @file    efts_topics.h
 * @brief   Efts MQTT topic segment / method / property name constants.
 */
#ifndef EFTS_TOPICS_H
#define EFTS_TOPICS_H

#define EFTS_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define EFTS_TOPIC_OWNER     "uav"
#define EFTS_TOPIC_CAP_NAME  "efts"

/* ---- service method names (payload.method) ---- */
#define EFTS_METHOD_SELECT_FTS_PWM_TRIGGER  "select_pwm_trigger"
#define EFTS_METHOD_GET_FTS_TRIGGER_STATUS  "get_trigger_status"



#ifdef __cplusplus
}
#endif
#endif /* EFTS_TOPICS_H */
