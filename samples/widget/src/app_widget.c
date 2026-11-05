/*
 * app_widget.c — widget business-logic implementation.
 */
#include "app_widget.h"
#include "widget_md5.h"

#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client/thing_model_client_codec.h"   /* ThingModelJson_* parses the config */

#include "thing_model_client_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#  define APP_WIDGET_PLATFORM_POSIX 1
#  include <time.h>
#  include <dirent.h>
#  include <sys/stat.h>
#else
#  define APP_WIDGET_PLATFORM_POSIX 0
#  include "widget_config_embedded.h"
#endif

/* ---- internal helpers -------------------------------------------------- */

static AppWidgetSlot *find_slot(AppWidget *app, int index)
{
    for (int i = 0; i < app->slot_count; i++)
        if (app->slots[i].index == index) return &app->slots[i];
    return NULL;
}

static void update_last_action(AppWidget *app, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(app->last_action, sizeof(app->last_action), fmt, ap);
    va_end(ap);

#if APP_WIDGET_PLATFORM_POSIX
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t)
        snprintf(app->last_action_time, sizeof(app->last_action_time),
                 "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
#else
    const T_ThingModelOsal *osal = ThingModelOsal_Get();
    uint64_t ms = (osal && osal->time_ms) ? osal->time_ms() : 0;
    snprintf(app->last_action_time, sizeof(app->last_action_time),
             "t=%llus", (unsigned long long)(ms / 1000));
#endif
}

static char *read_file_contents(const char *path)
{
#if APP_WIDGET_PLATFORM_POSIX
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t rd = fread(buf, 1, (size_t)size, fp);
    buf[rd] = '\0';
    fclose(fp);
    return buf;
#else
    (void)path;
    return NULL;
#endif
}

int app_widget_get_embedded_file(const char *name, const uint8_t **data, uint32_t *size)
{
#if APP_WIDGET_PLATFORM_POSIX
    (void)name;
    (void)data;
    (void)size;
    return 0;
#else
    const T_WidgetEmbeddedFile *file = widget_find_embedded_file(name);
    if (!file || !data || !size) return 0;
    *data = file->data;
    *size = file->size;
    return 1;
#endif
}

/* ---- init / config loading ----------------------------------------------- */

void app_widget_init(AppWidget *app, const char *config_root)
{
    memset(app, 0, sizeof(*app));
    snprintf(app->config_root, sizeof(app->config_root), "%s",
             config_root ? config_root : "./config");
    snprintf(app->current_lang, sizeof(app->current_lang), "en");
    snprintf(app->last_action, sizeof(app->last_action), "none");
}

/* app_language enum → language directory name (corresponds to config/<lang>/ on disk). */
static const char *lang_dir(E_AppLanguage lang)
{
    switch (lang) {
        case APP_LANGUAGE_CHINESE_SIMPLIFIED: return "zh";
        case APP_LANGUAGE_JAPANESE:           return "ja";
        case APP_LANGUAGE_FRENCH:             return "fr";
        case APP_LANGUAGE_ENGLISH:
        default:                              return "en";
    }
}

/* Parse one widget_list array, appending each item to the table. */
static void parse_widget_array(AppWidget *app, const ThingModelJsonObj_t *arr)
{
    if (!arr || !ThingModelJson_IsArray(arr)) return;

    int n = ThingModelJson_GetArraySize(arr);
    for (int i = 0; i < n; i++) {
        if (app->slot_count >= APP_WIDGET_MAX) break;

        const ThingModelJsonObj_t *item = ThingModelJson_GetArrayItem(arr, i);
        if (!item) continue;
        const ThingModelJsonObj_t *idx  = ThingModelJson_GetItem(item, "widget_index");
        const ThingModelJsonObj_t *type = ThingModelJson_GetItem(item, "widget_type");
        if (!idx || !ThingModelJson_IsNumber(idx)) continue;

        int index = (int)ThingModelJson_GetInt(idx, 0);
        const char *type_str = ThingModelJson_GetString(type, "button");

        AppWidgetSlot *s = &app->slots[app->slot_count++];
        s->index = index;
        snprintf(s->type, sizeof(s->type), "%s", type_str);
        s->value = 1;   /* default initial value 1 */
        if (index >= 0 && index < APP_WIDGET_MAX)
            app->values[index] = 1;
    }
}

int app_widget_load_config(AppWidget *app, const char *json_path)
{
    (void)json_path;
    app->slot_count = 0;
    memset(app->values, 0, sizeof(app->values));

#if APP_WIDGET_PLATFORM_POSIX
    char *json = read_file_contents(json_path);
    if (!json) {
        LOG_ERROR("[widget] load_config: read %s FAIL", json_path);
        return -1;
    }
#else
    const char *json = (const char *)widget_config_json_fileBinaryArray;
#endif

    ThingModelJsonObj_t *root = ThingModelJson_Parse(json);
#if APP_WIDGET_PLATFORM_POSIX
    free(json);
#endif
    if (!root) {
        LOG_ERROR("[widget] load_config: parse FAIL");
        return -1;
    }

    const ThingModelJsonObj_t *main_if = ThingModelJson_GetItem(root, "main_interface");
    if (main_if)
        parse_widget_array(app, ThingModelJson_GetItem(main_if, "widget_list"));

    const ThingModelJsonObj_t *cfg_if = ThingModelJson_GetItem(root, "config_interface");
    if (cfg_if)
        parse_widget_array(app, ThingModelJson_GetItem(cfg_if, "widget_list"));

    ThingModelJson_Free(root);
    if (!app->quiet)
        LOG_INFO("[widget] load_config: %d widgets loaded", app->slot_count);
    return 0;
}

/* ---- state snapshot -------------------------------------------------------- */

void app_widget_fill_state(const AppWidget *app, T_WidgetStatePush *push, T_WidgetStateItem *items)
{
    for (int i = 0; i < app->slot_count; i++) {
        items[i].widget_index = app->slots[i].index;
        items[i].widget_type  = (char *)app->slots[i].type;
        items[i].widget_value = app->slots[i].value;
    }
    push->widget_list = items;
    push->widget_list_count = (size_t)app->slot_count;
}

/* ---- handlers --------------------------------------------------------- */

T_ThingModelReturnCode app_widget_on_get_file_list(WidgetImpl_t *impl, void *user,
        const T_WidgetGetFileListRequest *req, T_WidgetGetFileListReply *reply)
{
    (void)impl;
    AppWidget *app = user;
    app->n_get_file_list++;

    static int s_first = 1;

    E_AppLanguage lang_enum   = req ? req->language    : APP_LANGUAGE_ENGLISH;
    E_ScreenType  screen_enum = req ? req->screen_type : SCREEN_TYPE_LARGE;
    const char *lang = lang_dir(lang_enum);
    snprintf(app->current_lang, sizeof(app->current_lang), "%s", lang);

    char dir[400];
    snprintf(dir, sizeof(dir), "%s/%s", app->config_root, lang);

    if (!app->quiet && s_first) {
        LOG_INFO("[widget] get_file_list language=%d(%s) screen_type=%d",
               (int)lang_enum, lang, (int)screen_enum);
    }

#if APP_WIDGET_PLATFORM_POSIX
    DIR *d = opendir(dir);
    if (!d) {
        reply->result = -1;
        reply->result_message = ThingModelMem_Strdup("directory not found");
        update_last_action(app, "get_file_list lang=%s FAIL", lang);
        s_first = 0;
        return THING_MODEL_RC_OK;
    }

    struct dirent *e;
    int count = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char full[700];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat sb;
        if (stat(full, &sb) == 0 && S_ISREG(sb.st_mode)) count++;
    }

    T_WidgetFileInfo *arr = NULL;
    int idx = 0;
    if (count > 0) {
        arr = ThingModelMem_Calloc((size_t)count, sizeof(*arr));
        if (!arr) { closedir(d); s_first = 0; return THING_MODEL_RC_OUT_OF_MEMORY; }
        rewinddir(d);
        while ((e = readdir(d)) != NULL && idx < count) {
            if (e->d_name[0] == '.') continue;
            char full[700];
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            struct stat sb;
            if (stat(full, &sb) != 0 || !S_ISREG(sb.st_mode)) continue;
            char hex[33];
            if (widget_md5_file(full, hex) != 0) hex[0] = '\0';
            arr[idx].file_index = idx;
            arr[idx].file_name  = ThingModelMem_Strdup(e->d_name);
            arr[idx].file_size  = (int)sb.st_size;
            arr[idx].md5        = ThingModelMem_Strdup(hex);
            if (!app->quiet && s_first)
                LOG_INFO("[widget]   [%d] %s size=%ld md5=%s", idx, e->d_name, (long)sb.st_size, hex);
            idx++;
        }
    }
    closedir(d);

    reply->result         = 0;
    reply->result_message = ThingModelMem_Strdup("success");
    reply->file_list      = arr;
    reply->file_list_count = (size_t)idx;
    update_last_action(app, "get_file_list lang=%s files=%d OK", lang, idx);
#else
    {
        int n = WIDGET_EMBEDDED_FILE_COUNT;
        T_WidgetFileInfo *arr = ThingModelMem_Calloc((size_t)n, sizeof(*arr));
        if (arr) {
            for (int i = 0; i < n; i++) {
                const T_WidgetEmbeddedFile *ef = &s_widget_embedded_files[i];
                arr[i].file_index = i;
                arr[i].file_name  = ThingModelMem_Strdup(ef->name);
                arr[i].file_size  = (int)ef->size;
                arr[i].md5        = ThingModelMem_Strdup(ef->md5);
                if (s_first)
                    LOG_INFO("[widget]   [%d] %s size=%u md5=%s",
                           i, ef->name, (unsigned)ef->size, ef->md5);
            }
        }
        reply->result         = 0;
        reply->result_message = ThingModelMem_Strdup("success");
        reply->file_list      = arr;
        reply->file_list_count = arr ? (size_t)n : 0;
        update_last_action(app, "get_file_list lang=%s files=%d (RTOS)", lang, n);
    }
#endif
    if (s_first) {
        LOG_INFO("[widget] get_file_list done: %d files", (int)reply->file_list_count);
        s_first = 0;
    }
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_widget_on_set_value(WidgetImpl_t *impl, void *user,
        const T_WidgetSetValueRequest *req, T_ThingModelCommonReply *reply)
{
    (void)impl;
    AppWidget *app = user;
    app->n_set_value++;

    LOG_INFO("[widget] set_value index=%d type=%d value=%d",
           req->widget_index, req->widget_type, req->widget_value);

    if (req->widget_index < 0 || req->widget_index >= APP_WIDGET_MAX) {
        reply->result = -1;
        snprintf(reply->result_message, sizeof(reply->result_message), "widget_index out of range");
        update_last_action(app, "set_widget index=%d FAIL", req->widget_index);
        return THING_MODEL_RC_OK;
    }

    app->values[req->widget_index] = req->widget_value;
    AppWidgetSlot *s = find_slot(app, req->widget_index);
    if (s) s->value = req->widget_value;

    reply->result = 0;
    snprintf(reply->result_message, sizeof(reply->result_message), "success");
    update_last_action(app, "set_widget index=%d type=%d value=%d OK",
                       req->widget_index, req->widget_type, req->widget_value);
    return THING_MODEL_RC_OK;
}

T_ThingModelReturnCode app_widget_on_get_value(WidgetImpl_t *impl, void *user,
        const T_WidgetGetValueRequest *req, T_WidgetGetValueReply *reply)
{
    (void)impl;
    AppWidget *app = user;
    app->n_get_value++;

    LOG_INFO("[widget] get_value index=%d", req->widget_index);
    reply->widget_index = req->widget_index;

    if (req->widget_index < 0 || req->widget_index >= APP_WIDGET_MAX) {
        reply->result = -1;
        reply->result_message = ThingModelMem_Strdup("widget_index out of range");
        reply->widget_value = 0;
        return THING_MODEL_RC_OK;
    }

    reply->result = 0;
    reply->result_message = ThingModelMem_Strdup("success");
    reply->widget_value = app->values[req->widget_index];
    update_last_action(app, "get_widget index=%d value=%d OK",
                       req->widget_index, reply->widget_value);
    return THING_MODEL_RC_OK;
}
