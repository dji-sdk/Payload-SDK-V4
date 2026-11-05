/**
 * @file    upgrade_topics.h
 * @brief   Upgrade MQTT topic segment / method / property name constants.
 */
#ifndef UPGRADE_TOPICS_H
#define UPGRADE_TOPICS_H

#define UPGRADE_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define UPGRADE_TOPIC_OWNER     "uav"
#define UPGRADE_TOPIC_CAP_NAME  "upgrade"

/* ---- service method names (payload.method) ---- */
#define UPGRADE_METHOD_ENTER_UPGRADE_MODE  "enter_upgrade_mode"
#define UPGRADE_METHOD_CHECK_FIRMWARE  "check_firmware"
#define UPGRADE_METHOD_START_UPGRADE  "start_upgrade"
#define UPGRADE_METHOD_FINISH_UPGRADE  "finish_upgrade"

/* ---- push method names (event/osd/state payload.method) ---- */
#define UPGRADE_METHOD_PUSH_UPGRADE_STATE  "push_upgrade_state"


#ifdef __cplusplus
}
#endif
#endif /* UPGRADE_TOPICS_H */
