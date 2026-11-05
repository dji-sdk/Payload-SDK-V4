/*
 * app_widget.h — widget business-logic module (decoupled from transport/platform for MCU reuse).
 *
 * Holds the widget value table + dynamically loads the widget list from widget_config.json; provides 3
 * WidgetImpl service handlers and the state push snapshot builder.
 * File/directory scanning (get_file_list) uses POSIX, concentrated in this file, so porting only needs to replace this one spot.
 */
#ifndef APP_WIDGET_H
#define APP_WIDGET_H

#include "widget/widget_impl.h"
#include "widget/widget_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_WIDGET_MAX        64   /* upper bound for widget_index */
#define APP_WIDGET_TYPE_LEN   32

typedef struct {
    int  index;
    char type[APP_WIDGET_TYPE_LEN];   /* button/list/switch/scale/int_input_box */
    int  value;
} AppWidgetSlot;

typedef struct {
    AppWidgetSlot slots[APP_WIDGET_MAX];
    int           slot_count;
    int           values[APP_WIDGET_MAX];   /* stored directly by index, 0..MAX-1 */
    char          config_root[256];         /* widget config root directory, e.g. "./config" */
    char          current_lang[16];         /* language directory name (en/zh/ja/fr) selected by the most recent get_file_list; reused by the HTTP server */
    int           quiet;                     /* 1=silence business logs (load_config/get_file_list detail); for the background upgrade widget, default 0 */

    /* most recent action (for the float window display) */
    char last_action[256];
    char last_action_time[16];

    /* count of received requests (for observation) */
    volatile int n_get_file_list;
    volatile int n_set_value;
    volatile int n_get_value;
} AppWidget;

/** Initialize (zero out + record the config root directory). */
void app_widget_init(AppWidget *app, const char *config_root);

/** Parse the widget_list of main_interface + config_interface from widget_config.json,
 *  building the table dynamically (index/type, initial value 1). Returns -1 on failure. */
int  app_widget_load_config(AppWidget *app, const char *json_path);

/** Fill push with the current widget value snapshot (items provided by the caller, capacity >= APP_WIDGET_MAX). */
void app_widget_fill_state(const AppWidget *app, T_WidgetStatePush *push, T_WidgetStateItem *items);

int app_widget_get_embedded_file(const char *name, const uint8_t **data, uint32_t *size);

/* ---- WidgetImpl handlers (user passes AppWidget* at registration) ---- */
T_ThingModelReturnCode app_widget_on_get_file_list(WidgetImpl_t *impl, void *user,
        const T_WidgetGetFileListRequest *req, T_WidgetGetFileListReply *reply);
T_ThingModelReturnCode app_widget_on_set_value(WidgetImpl_t *impl, void *user,
        const T_WidgetSetValueRequest *req, T_ThingModelCommonReply *reply);
T_ThingModelReturnCode app_widget_on_get_value(WidgetImpl_t *impl, void *user,
        const T_WidgetGetValueRequest *req, T_WidgetGetValueReply *reply);

#ifdef __cplusplus
}
#endif

#endif /* APP_WIDGET_H */
