/**
 * @file    widget_types.h
 * @brief   Widget enums + structs + init functions (DO NOT EDIT).
 */
#ifndef WIDGET_TYPES_H
#define WIDGET_TYPES_H

#include "thing_model_client/thing_model_client_types.h"
#include <string.h>
#include "common/common_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_LANGUAGE_ENGLISH = 0,
    APP_LANGUAGE_CHINESE_SIMPLIFIED = 1,
    APP_LANGUAGE_JAPANESE = 2,
    APP_LANGUAGE_FRENCH = 3,
} E_AppLanguage;

typedef enum {
    SCREEN_TYPE_LARGE = 0,
    SCREEN_TYPE_SMALL = 1,
} E_ScreenType;


typedef struct {
  int32_t  file_index;
  char     *file_name;
  int32_t  file_size;
  char     *md5;
} T_WidgetFileInfo;

typedef struct {
  E_AppLanguage language;
  E_ScreenType  screen_type;
} T_WidgetGetFileListRequest;

typedef struct {
  int32_t          result;
  char             *result_message;
  T_WidgetFileInfo *file_list;
  size_t           file_list_count;
} T_WidgetGetFileListReply;

typedef struct {
  int32_t widget_index;
  int32_t widget_type;
  int32_t widget_value;
} T_WidgetSetValueRequest;

typedef struct {
  int32_t widget_index;
} T_WidgetGetValueRequest;

typedef struct {
  int32_t  result;
  char     *result_message;
  int32_t  widget_index;
  int32_t  widget_value;
} T_WidgetGetValueReply;

typedef struct {
  int32_t  widget_index;
  char     *widget_type;
  int32_t  widget_value;
} T_WidgetStateItem;

typedef struct {
  T_WidgetStateItem *widget_list;
  size_t            widget_list_count;
} T_WidgetStatePush;

typedef struct {
  char     *message;
} T_WidgetFloatWindowPush;


static inline void Widget_InitWidgetFileInfo(T_WidgetFileInfo *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetGetFileListRequest(T_WidgetGetFileListRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetGetFileListReply(T_WidgetGetFileListReply *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetSetValueRequest(T_WidgetSetValueRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetGetValueRequest(T_WidgetGetValueRequest *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetGetValueReply(T_WidgetGetValueReply *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetStateItem(T_WidgetStateItem *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetStatePush(T_WidgetStatePush *p) { if (p) memset(p, 0, sizeof(*p)); }
static inline void Widget_InitWidgetFloatWindowPush(T_WidgetFloatWindowPush *p) { if (p) memset(p, 0, sizeof(*p)); }

#ifdef __cplusplus
}
#endif
#endif /* WIDGET_TYPES_H */
