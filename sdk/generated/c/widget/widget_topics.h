/**
 * @file    widget_topics.h
 * @brief   Widget MQTT topic segment / method / property name constants.
 */
#ifndef WIDGET_TOPICS_H
#define WIDGET_TOPICS_H

#define WIDGET_GENERATOR_VERSION  "0.3.0"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- topic segments: full topic = /thing/<owner>/<sn>/<cap_name>/<channel> ---- */
#define WIDGET_TOPIC_OWNER     "uav"
#define WIDGET_TOPIC_CAP_NAME  "widget"

/* ---- service method names (payload.method) ---- */
#define WIDGET_METHOD_GET_FILE_LIST  "get_file_list"
#define WIDGET_METHOD_SET_VALUE  "set_value"
#define WIDGET_METHOD_GET_VALUE  "get_value"

/* ---- push method names (event/osd/state payload.method) ---- */
#define WIDGET_METHOD_STATE  "state"
#define WIDGET_METHOD_FLOAT_WINDOW_MSG  "float_window_msg"


#ifdef __cplusplus
}
#endif
#endif /* WIDGET_TOPICS_H */
