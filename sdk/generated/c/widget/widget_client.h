/**
 * @file    widget_client.h
 * @brief   Widget client API: from the caller's perspective (cloud / RC / app).
 */
#ifndef WIDGET_CLIENT_H
#define WIDGET_CLIENT_H

#include "thing_model_client/thing_model_client_session.h"
#include "widget_types.h"
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Widget_Client Widget_Client_t;

Widget_Client_t *Widget_CreateClient (ThingModelClientSession_t *session);
void              Widget_DestroyClient(Widget_Client_t *c);

/** Protocol code version this SDK was built against. */
const char *Widget_GeneratorVersion(void);

/**
 * Build this cap's topic string from (style, sn, kind); the caller frees it.
 * Used for testing / dry-run / custom transport.
 */
char *Widget_BuildTopic(E_ThingModelTopicStyle style, const char *sn, E_ThingModelTopicKind kind);


/* ============================================================ */
/* SERVICE: actions (sync blocks waiting for reply / async registers a callback)  */
/* ============================================================ */
/** GetFileList async callback — the decoded typed reply (proto declares returns (WidgetGetFileListReply)) */
typedef void (*Widget_GetFileList_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_WidgetGetFileListReply *reply);
/** GetValue async callback — the decoded typed reply (proto declares returns (WidgetGetValueReply)) */
typedef void (*Widget_GetValue_AsyncCb)(void *user, T_ThingModelReturnCode status, const T_WidgetGetValueReply *reply);

T_ThingModelReturnCode Widget_GetFileList(Widget_Client_t *c, const T_WidgetGetFileListRequest *req, T_WidgetGetFileListReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Widget_GetFileListAsync(Widget_Client_t *c, const T_WidgetGetFileListRequest *req, Widget_GetFileList_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Widget_BuildGetFileListPayload(E_ThingModelTopicStyle style, uint32_t seq, const T_WidgetGetFileListRequest *req);
T_ThingModelReturnCode Widget_SetValue(Widget_Client_t *c, const T_WidgetSetValueRequest *req, T_ThingModelCommonReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Widget_SetValueAsync(Widget_Client_t *c, const T_WidgetSetValueRequest *req, T_ThingModelAsyncReplyCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Widget_BuildSetValuePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_WidgetSetValueRequest *req);
T_ThingModelReturnCode Widget_GetValue(Widget_Client_t *c, const T_WidgetGetValueRequest *req, T_WidgetGetValueReply *resp, uint32_t timeout_ms);
T_ThingModelReturnCode Widget_GetValueAsync(Widget_Client_t *c, const T_WidgetGetValueRequest *req, Widget_GetValue_AsyncCb cb, void *user);
/** Build but do not publish. Returns a caller-free JSON string; used for testing / dry-run / custom transport. */
char *Widget_BuildGetValuePayload(E_ThingModelTopicStyle style, uint32_t seq, const T_WidgetGetValueRequest *req);




/* ============================================================ */
/* push subscription (EVENT / OSD / STATE)                       */
/* ============================================================ */
typedef void (*Widget_State_Cb)(void *user, const T_WidgetStatePush *push);
T_ThingModelReturnCode Widget_OnState(Widget_Client_t *c, Widget_State_Cb cb, void *user);
typedef void (*Widget_FloatWindowMsg_Cb)(void *user, const T_WidgetFloatWindowPush *push);
T_ThingModelReturnCode Widget_OnFloatWindowMsg(Widget_Client_t *c, Widget_FloatWindowMsg_Cb cb, void *user);

#ifdef __cplusplus
}
#endif
#endif /* WIDGET_CLIENT_H */
