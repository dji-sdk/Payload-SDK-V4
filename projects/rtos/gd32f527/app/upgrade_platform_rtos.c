#include "upgrade_platform.h"
#include "upgrade_rtos_storage.h"
#include "upgrade_rtos_http.h"
#include "thing_model_client_log.h"
#include "gd32f5xx.h"

#include <stdio.h>
#include <string.h>

int upgrade_platform_storage_ok(long *size_out)
{
    if (size_out) *size_out = (long)UpgradeRtosStorage_Size();
    return UpgradeRtosStorage_IsReady();
}

int upgrade_platform_http_start(const char *fw_name, const char *iface,
                                char *url_out, size_t url_cap)
{
    (void)fw_name; (void)iface;
    UpgradeRtosHttp_Start();
    snprintf(url_out, url_cap, "%s", UpgradeRtosHttp_Url());
    return 0;
}

void upgrade_platform_http_stop(void) { }

/* RTOS image is streamed into staging Flash over HTTP before start_upgrade, so
 * there is no per-percent flash loop here — the loader does the real copy on the
 * next boot. */
int upgrade_platform_flash(upgrade_progress_cb_t cb) { (void)cb; return 0; }

int upgrade_platform_set_boot_pending(void) { return UpgradeRtosStorage_SetBootPending(); }

void upgrade_platform_reboot(void) { NVIC_SystemReset(); }

int upgrade_platform_has_pending_completion(void) { return UpgradeRtosStorage_HasPendingCompletion(); }

int upgrade_platform_consume_boot_pending(void) { return UpgradeRtosStorage_ConsumeBootPending(); }

int upgrade_platform_mark_completion_finished(void) { return UpgradeRtosStorage_MarkCompletionFinished(); }

/* RTOS never exits the sample — it resets state and waits for the next round. */
int upgrade_platform_round_complete(void) { return 0; }

void upgrade_platform_on_session_ready(ThingModelClientSession_t *session, const char *sn)
{
    (void)session; (void)sn;
}

void upgrade_platform_on_session_close(void) { }
