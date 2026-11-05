/**
 * @file    widget_json.h
 * @brief   Widget struct ↔ cJSON conversion.
 *
 * Conventions:
 *   Encode<M>(src)               → returns a malloc'd compact JSON string; the caller free()s it.
 *   Decode<M>(json, out)         → takes a JSON string, Parses it internally; fields written into *out; returns RC.
 *                                  The heap fields (string / bytes / repeated) in out are malloc'd by this function,
 *                                  and after use you must call <Cap>_Free<M>(out) to free them, otherwise memory leaks.
 *   <Cap>_Free<M>(p)             → recursively frees all heap fields inside *p; safe to call repeatedly (NULL+0 compatible);
 *                                  does not free p itself.
 *
 *   Encode<M>_AsObj / Decode<M>_FromObj are internal helpers (for same-package/cross-package codegen to nest calls into each other);
 *   business code should use the main Encode/Decode/Free.
 */
#ifndef WIDGET_JSON_H
#define WIDGET_JSON_H

#include "widget_types.h"
#include "thing_model_client/thing_model_client_codec.h"
#include "common/common_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- service/property common reply ---- */
T_ThingModelReturnCode WidgetJson_DecodeCommonReply(const char *json, T_ThingModelCommonReply *out);
T_ThingModelReturnCode WidgetJson_DecodePropSetReply(const char *json, const char *prop, T_ThingModelCommonReply *out);

/* ---- request/property encoding utilities ---- */
char *WidgetJson_EncodeServiceRequest(uint32_t seq, const char *method, const char *data_json);
char *WidgetJson_EncodePropertyGet(uint32_t seq, const char *const *props, int n_props);
char *WidgetJson_EncodePropertySetRaw(uint32_t seq, const char *data_json);

/* ---- per-message encode / decode / free ---- */
char *           WidgetJson_EncodeWidgetFileInfo(const T_WidgetFileInfo *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetFileInfo(const char *json, T_WidgetFileInfo *out);
void             Widget_FreeWidgetFileInfo(T_WidgetFileInfo *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetFileInfo_AsObj(const T_WidgetFileInfo *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetFileInfo_FromObj(const ThingModelJsonObj_t *obj, T_WidgetFileInfo *out);
char *           WidgetJson_EncodeWidgetGetFileListRequest(const T_WidgetGetFileListRequest *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetGetFileListRequest(const char *json, T_WidgetGetFileListRequest *out);
void             Widget_FreeWidgetGetFileListRequest(T_WidgetGetFileListRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetGetFileListRequest_AsObj(const T_WidgetGetFileListRequest *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListRequest_FromObj(const ThingModelJsonObj_t *obj, T_WidgetGetFileListRequest *out);
char *           WidgetJson_EncodeWidgetGetFileListReply(const T_WidgetGetFileListReply *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetGetFileListReply(const char *json, T_WidgetGetFileListReply *out);
void             Widget_FreeWidgetGetFileListReply(T_WidgetGetFileListReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetGetFileListReply_AsObj(const T_WidgetGetFileListReply *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetGetFileListReply_FromObj(const ThingModelJsonObj_t *obj, T_WidgetGetFileListReply *out);
char *           WidgetJson_EncodeWidgetSetValueRequest(const T_WidgetSetValueRequest *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetSetValueRequest(const char *json, T_WidgetSetValueRequest *out);
void             Widget_FreeWidgetSetValueRequest(T_WidgetSetValueRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetSetValueRequest_AsObj(const T_WidgetSetValueRequest *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetSetValueRequest_FromObj(const ThingModelJsonObj_t *obj, T_WidgetSetValueRequest *out);
char *           WidgetJson_EncodeWidgetGetValueRequest(const T_WidgetGetValueRequest *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetGetValueRequest(const char *json, T_WidgetGetValueRequest *out);
void             Widget_FreeWidgetGetValueRequest(T_WidgetGetValueRequest *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetGetValueRequest_AsObj(const T_WidgetGetValueRequest *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueRequest_FromObj(const ThingModelJsonObj_t *obj, T_WidgetGetValueRequest *out);
char *           WidgetJson_EncodeWidgetGetValueReply(const T_WidgetGetValueReply *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetGetValueReply(const char *json, T_WidgetGetValueReply *out);
void             Widget_FreeWidgetGetValueReply(T_WidgetGetValueReply *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetGetValueReply_AsObj(const T_WidgetGetValueReply *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetGetValueReply_FromObj(const ThingModelJsonObj_t *obj, T_WidgetGetValueReply *out);
char *           WidgetJson_EncodeWidgetStateItem(const T_WidgetStateItem *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetStateItem(const char *json, T_WidgetStateItem *out);
void             Widget_FreeWidgetStateItem(T_WidgetStateItem *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetStateItem_AsObj(const T_WidgetStateItem *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetStateItem_FromObj(const ThingModelJsonObj_t *obj, T_WidgetStateItem *out);
char *           WidgetJson_EncodeWidgetStatePush(const T_WidgetStatePush *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetStatePush(const char *json, T_WidgetStatePush *out);
void             Widget_FreeWidgetStatePush(T_WidgetStatePush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetStatePush_AsObj(const T_WidgetStatePush *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetStatePush_FromObj(const ThingModelJsonObj_t *obj, T_WidgetStatePush *out);
char *           WidgetJson_EncodeWidgetFloatWindowPush(const T_WidgetFloatWindowPush *src);
T_ThingModelReturnCode  WidgetJson_DecodeWidgetFloatWindowPush(const char *json, T_WidgetFloatWindowPush *out);
void             Widget_FreeWidgetFloatWindowPush(T_WidgetFloatWindowPush *p);

/* internal: used for nesting scenarios; business code should use the main API above. */
ThingModelJsonObj_t   *WidgetJson_EncodeWidgetFloatWindowPush_AsObj(const T_WidgetFloatWindowPush *src);
T_ThingModelReturnCode WidgetJson_DecodeWidgetFloatWindowPush_FromObj(const ThingModelJsonObj_t *obj, T_WidgetFloatWindowPush *out);

#ifdef __cplusplus
}
#endif
#endif /* WIDGET_JSON_H */
