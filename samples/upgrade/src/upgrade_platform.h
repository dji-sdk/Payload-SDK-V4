#ifndef UPGRADE_PLATFORM_H
#define UPGRADE_PLATFORM_H

#include <stddef.h>

typedef struct ThingModelClientSession ThingModelClientSession_t;

typedef int (*upgrade_progress_cb_t)(int percent);

int  upgrade_platform_storage_ok(long *size_out);
int  upgrade_platform_http_start(const char *fw_name, const char *iface,
                                 char *url_out, size_t url_cap);
void upgrade_platform_http_stop(void);
int  upgrade_platform_flash(upgrade_progress_cb_t cb);
int  upgrade_platform_set_boot_pending(void);
void upgrade_platform_reboot(void);
int  upgrade_platform_has_pending_completion(void);
int  upgrade_platform_consume_boot_pending(void);
int  upgrade_platform_mark_completion_finished(void);
int  upgrade_platform_round_complete(void);
void upgrade_platform_on_session_ready(ThingModelClientSession_t *session, const char *sn);
void upgrade_platform_on_session_close(void);

#endif
