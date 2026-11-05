/**
 * @file    widget_publisher.h
 * @brief   Widget publisher API: from the device side's perspective (aircraft side / simulator).
 */
#ifndef WIDGET_PUBLISHER_H
#define WIDGET_PUBLISHER_H

#include "thing_model_client/thing_model_client_session.h"
#include "widget_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Widget_Publisher Widget_Publisher_t;

Widget_Publisher_t *Widget_CreatePublisher (ThingModelClientSession_t *session);
void                              Widget_DestroyPublisher(Widget_Publisher_t *p);

/** Publish State (state topic). */
T_ThingModelReturnCode Widget_PublishState(Widget_Publisher_t *p, const T_WidgetStatePush *push);
/** Publish FloatWindowMsg (state topic). */
T_ThingModelReturnCode Widget_PublishFloatWindowMsg(Widget_Publisher_t *p, const T_WidgetFloatWindowPush *push);

#ifdef __cplusplus
}
#endif
#endif /* WIDGET_PUBLISHER_H */
