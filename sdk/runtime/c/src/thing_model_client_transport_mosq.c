/**
 * @file    thing_model_client_transport_mosq.c
 * @brief   libmosquitto implementation of T_ThingModelTransportVtable.
 *          - Supports TLS / non-TLS
 *          - On publish, if hmac_enabled, automatically appends [ts_be8|HMAC32]
 *          - Provides export_keying_material for secure_access
 */
#include "thing_model_client/thing_model_client_transport.h"
#include "thing_model_client/thing_model_client_osal.h"
#include "thing_model_client_secure_access.h"
#include "thing_model_client_log.h"

#include <mosquitto.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int s_lib_inited = 0;

struct T_ThingModelTransportImpl {
    struct mosquitto *mosq;
    SSL              *current_ssl;       /* mosquitto's internal SSL in TLS mode */
    SSL_CTX          *ssl_ctx;
    T_ThingModelTransportMsgCb  msg_cb;
    void              *msg_user;
    T_ThingModelTransportConnCb conn_cb;
    void              *conn_user;
    bool               hmac_enabled;
    uint8_t            hmac_key[32];
    const T_ThingModelCryptoVtable *crypto;
    void              *connect_event;    /* wait for CONNACK */
    int                connect_rc;
};

/* ---- Callbacks ---- */
static void on_connect_cb(struct mosquitto *m, void *obj, int rc)
{
    (void)m;
    T_ThingModelTransportImpl_t *t = obj;
    t->connect_rc = rc;
    if (rc == 0) {
        LOG_INFO("mosq CONNACK rc=0 (accepted)");
    } else {
        /* MQTT CONNACK error codes: 1=unaccept proto, 2=client_id rejected, 3=server unavailable,
         * 4=bad user/pass, 5=not authorized */
        LOG_ERROR("mosq CONNACK rc=%d (%s)", rc, mosquitto_connack_string(rc));
    }
    /* In TLS mode, by the time CONNACK is received, mosquitto has already completed the TLS handshake internally */
    if (t->ssl_ctx) {
        /* At this point the SSL object is accessible (mosquitto's internal net_ssl) */
        /* Since mosquitto 3.x the internal field is not exported; it can be obtained via mosquitto_ssl_get */
#if MOSQ_LIB_VERSION_MAJOR >= 2
        /* libmosquitto 2.x has no official API; the SSL_CTX is pre-set before connect
         * and the SSL is captured via the info callback. */
#endif
    }
    if (t->connect_event) ThingModelOsal_Get()->event_signal(t->connect_event);
    if (t->conn_cb) t->conn_cb(t->conn_user, rc);
}

static void on_disconnect_cb(struct mosquitto *m, void *obj, int rc)
{
    (void)m;
    T_ThingModelTransportImpl_t *t = obj;
    LOG_INFO("mosq disconnected rc=%d (%s)", rc, rc ? mosquitto_strerror(rc) : "clean");
    if (t->conn_cb) t->conn_cb(t->conn_user, rc ? rc : -1);
}

static void on_message_cb(struct mosquitto *m, void *obj, const struct mosquitto_message *msg)
{
    (void)m;
    T_ThingModelTransportImpl_t *t = obj;
    if (t->msg_cb)
        t->msg_cb(t->msg_user, msg->topic, msg->payload, (size_t)msg->payloadlen);
}

/* ---- Grab SSL* via the SSL_CTX info callback + log handshake-time diagnostics ----
 * Without this cb, a TLS handshake error is "silent" (mosquitto_connect succeeds synchronously → CONNACK wait times out,
 * with no visible alert/cert verify error). */
static void ssl_info_cb(const SSL *ssl, int where, int ret)
{
    if (where & SSL_CB_HANDSHAKE_START) {
        LOG_INFO("[TLS] handshake start");
    }
    if (where & SSL_CB_HANDSHAKE_DONE) {
        /* SSL_get_app_data() queries per-SSL app data, but we set it at the CTX level, so we route around it */
        SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
        T_ThingModelTransportImpl_t *t = ctx ? SSL_CTX_get_app_data(ctx) : NULL;
        if (t) t->current_ssl = (SSL *)ssl;
        long verify = SSL_get_verify_result(ssl);
        LOG_INFO("[TLS] handshake done, version=%s cipher=%s verify=%ld (%s)",
                 SSL_get_version(ssl),
                 SSL_get_cipher_name(ssl) ? SSL_get_cipher_name(ssl) : "?",
                 verify,
                 verify == X509_V_OK ? "ok" : X509_verify_cert_error_string(verify));
    }
    /* alert sent or received — distinguish warning (normal signals like close_notify) vs fatal (real errors).
     * OpenSSL info_cb's ret encoding: high byte=level (1=warning, 2=fatal), low byte=desc.
     * SSL_alert_type_string_long returns "warning" or "fatal". */
    if (where & SSL_CB_ALERT) {
        const char *dir   = (where & SSL_CB_READ) ? "recv" : "send";
        const char *level = SSL_alert_type_string_long(ret);
        const char *desc  = SSL_alert_desc_string_long(ret);
        if (level && level[0] == 'f') {  /* "fatal" */
            LOG_ERROR("[TLS] alert %s: type=%s desc=%s", dir, level, desc);
        } else {
            LOG_INFO("[TLS] alert %s: type=%s desc=%s", dir, level, desc);
        }
    }
    /* exit within loop — distinguish "waiting for more data" (normal) vs a real error.
     * SSL_connect/SSL_accept returning < 0 is mostly WANT_READ/WANT_WRITE; the handshake isn't done
     * yet, which is "in progress" not fail; a real fail is still reported via the alert path. */
    if (where & SSL_CB_EXIT) {
        if (ret == 0) {
            LOG_ERROR("[TLS] EOF during handshake — broker closed the connection, possibly not accepting our ClientHello "
                      "(cipher/SNI/protocol mismatch)");
        } else if (ret < 0) {
            int err = SSL_get_error(ssl, ret);
            if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                LOG_ERROR("[TLS] handshake error rc=%d SSL_get_error=%d", ret, err);
            }
            /* WANT_READ/WANT_WRITE = normally waiting for data; don't log to avoid flooding */
        }
    }
}

/* ---- vtable impls ---- */
static T_ThingModelTransportImpl_t *m_create(void)
{
    if (!s_lib_inited) { mosquitto_lib_init(); s_lib_inited = 1; }
    T_ThingModelTransportImpl_t *t = calloc(1, sizeof(*t));
    return t;
}

static void m_destroy(T_ThingModelTransportImpl_t *t)
{
    if (!t) return;
    if (t->mosq) {
        /* Graceful stop, not force=true (async cancel) — see the note in m_disconnect().
         * disconnect() first so the loop thread exits on its own, then join it cleanly. */
        mosquitto_disconnect(t->mosq);
        mosquitto_loop_stop(t->mosq, false);
        mosquitto_destroy(t->mosq);
    }
    if (t->ssl_ctx) SSL_CTX_free(t->ssl_ctx);
    if (t->connect_event) ThingModelOsal_Get()->event_destroy(t->connect_event);
    free(t);
}

static void m_set_message_cb(T_ThingModelTransportImpl_t *t, T_ThingModelTransportMsgCb cb, void *u)
{
    t->msg_cb = cb; t->msg_user = u;
}
static void m_set_conn_cb(T_ThingModelTransportImpl_t *t, T_ThingModelTransportConnCb cb, void *u)
{
    t->conn_cb = cb; t->conn_user = u;
}

static T_ThingModelReturnCode m_connect(T_ThingModelTransportImpl_t *t, const T_ThingModelTransportConfig *cfg)
{
    if (!t || !cfg) return THING_MODEL_RC_INVALID_ARG;
    const T_ThingModelOsal *o = ThingModelOsal_Get();

    if (t->mosq) { mosquitto_destroy(t->mosq); t->mosq = NULL; }
    t->mosq = mosquitto_new(cfg->client_id, cfg->clean_session, t);
    if (!t->mosq) return THING_MODEL_RC_OUT_OF_MEMORY;

    /* Note: do **not** call mosquitto_threaded_set(true).
     * That API tells the lib "external threading drives it", which sets mosq->threaded to
     * mosq_ts_external, causing the later mosquitto_loop_start() to report MOSQ_ERR_INVAL
     * (loop_start requires threaded==none to switch to mosq_ts_self).
     * loop_start starts its own thread with an internal mutex, so no prior threaded_set is needed. */

    mosquitto_connect_callback_set   (t->mosq, on_connect_cb);
    mosquitto_disconnect_callback_set(t->mosq, on_disconnect_cb);
    mosquitto_message_callback_set   (t->mosq, on_message_cb);

    if (cfg->username) {
        mosquitto_username_pw_set(t->mosq, cfg->username, cfg->password);
    }

    if (cfg->use_tls) {
        /* CA guard: when there's no CA, fail early, and never enter SSL_CTX allocation / mosquitto_tls_set.
         * The CA guard prevents two failure modes:
         *   ① mosquitto_tls_set(mosq, NULL, NULL, NULL, NULL, NULL) all NULL → MOSQ_ERR_INVAL(rc=3).
         *   ② Segfault: ssl_ctx is already built and handed to mosquitto via MOSQ_OPT_SSL_CTX, but the tls_set failure means
         *      the TLS handshake never happens → mosquitto's user_ssl_ctx ownership flag (set only in net__init_ssl_ctx) is not set,
         *      mosquitto_destroy frees that ctx, then m_destroy SSL_CTX_free's it again → double free.
         * Returning early means ssl_ctx is never allocated, eliminating both defects at once. For plaintext access set plaintext_access=true. */
        if (!cfg->ca_cert_path || !cfg->ca_cert_path[0]) {
            LOG_ERROR("[SEC] use_tls but no ca_cert_path provided — TLS requires a CA (mosquitto_tls_set). "
                      "For plaintext access, set plaintext_access=true.");
            return THING_MODEL_RC_INVALID_ARG;
        }
        /* Build our own SSL_CTX to grab the SSL —— flow aligned with the legacy C++ SDK/mqtt_client/src/secure_access_client.cpp */
        if (t->ssl_ctx) SSL_CTX_free(t->ssl_ctx);
        t->ssl_ctx = SSL_CTX_new(TLS_client_method());
        SSL_CTX_set_options(t->ssl_ctx,
                            SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
#ifdef SSL_OP_NO_TLSv1_3
        SSL_CTX_set_options(t->ssl_ctx, SSL_OP_NO_TLSv1_3);
#endif
        if (cfg->ca_cert_path) {
            if (SSL_CTX_load_verify_locations(t->ssl_ctx, cfg->ca_cert_path, NULL) != 1) {
                LOG_ERROR("[SEC] SSL_CTX_load_verify_locations FAILED: %s", cfg->ca_cert_path);
                return THING_MODEL_RC_AUTH_FAILED;
            }
        }
        SSL_CTX_set_verify(t->ssl_ctx, SSL_VERIFY_PEER, NULL);

        /* Skip the certificate validity-period (notBefore/notAfter) check —— only relaxes the "time window";
         * the cert chain/signature/hostname are still strictly verified. Devices with no RTC or a wrong
         * boot-time system clock (like a bare Raspberry Pi) would otherwise fail the handshake with
         * X509_V_ERR_CERT_NOT_YET_VALID. */
        {
            X509_VERIFY_PARAM *vp = SSL_CTX_get0_param(t->ssl_ctx);
            if (vp) X509_VERIFY_PARAM_set_flags(vp, X509_V_FLAG_NO_CHECK_TIME);
        }

        SSL_CTX_set_info_callback(t->ssl_ctx, ssl_info_cb);

        /* Attach the transport pointer to all new SSLs (info_cb retrieves it via SSL_get_app_data) */
        SSL_CTX_set_app_data(t->ssl_ctx, t);

        LOG_INFO("[SEC] Phase1: SSL_CTX configured (TLSv1.2 only, verify peer, CA=%s)",
                 cfg->ca_cert_path ? cfg->ca_cert_path : "(NONE)");

        /* Give our own ctx to mosquitto. Do **not** set MOSQ_OPT_SSL_CTX_WITH_DEFAULTS=0 ——
         * consistent with the reference, default 1, letting mosquitto layer its own cipher defaults on top.
         * mosquitto_tls_set is still needed, to tell mosquitto "this is a TLS connection". */
        mosquitto_opts_set(t->mosq, MOSQ_OPT_SSL_CTX, t->ssl_ctx);
        int trc = mosquitto_tls_set(t->mosq, cfg->ca_cert_path, NULL, NULL, NULL, NULL);
        if (trc != MOSQ_ERR_SUCCESS) {
            LOG_ERROR("[SEC] mosquitto_tls_set FAILED: %s (rc=%d)", mosquitto_strerror(trc), trc);
            return THING_MODEL_RC_AUTH_FAILED;
        }
        /* tls_cert_reqs=0: prevent mosquitto from installing its own verify callback that overrides ours;
         * "tlsv1.2" as a double safeguard (our SSL_CTX already restricted it). */
        mosquitto_tls_opts_set(t->mosq, 0, "tlsv1.2", NULL);
    }

    /* HMAC configuration */
    t->hmac_enabled = cfg->hmac_enabled;
    if (cfg->hmac_enabled && cfg->hmac_key)
        memcpy(t->hmac_key, cfg->hmac_key, 32);
    t->crypto = cfg->crypto;     /* passed in by session, may be NULL (in which case hmac_enabled should not be true) */

    /* Prepare the event to wait for CONNACK */
    if (!t->connect_event) t->connect_event = o->event_create();
    t->connect_rc = -1;

    int rc = mosquitto_connect(t->mosq, cfg->host, cfg->port,
                               cfg->keepalive_sec ? cfg->keepalive_sec : 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("mosquitto_connect %s:%u failed: %d %s (errno=%d %s)",
                  cfg->host, cfg->port, rc, mosquitto_strerror(rc),
                  errno, strerror(errno));
        return THING_MODEL_RC_TRANSPORT_ERROR;
    }
    LOG_INFO("mosq tcp connected; driving handshake + CONNACK (sync loop, 5s budget) ...");

    /* Synchronously drive mosquitto_loop() to complete the TLS handshake + wait for CONNACK, consistent with
     * the legacy C++ SDK/mqtt_client/src/secure_access_client.cpp.
     * The handshake and CONNACK are driven synchronously via mosquitto_loop() here; a background thread
     * (loop_start) would not service the socket during the handshake, stalling it after ClientHello.
     * Only after the handshake passes does loop_start() start the background thread to take over subsequent publish/subscribe. */
    (void)o;
    for (int i = 0; i < 50 && t->connect_rc == -1; i++) {  /* 50 × 100ms = 5s */
        int lr = mosquitto_loop(t->mosq, 100, 1);
        if (lr != MOSQ_ERR_SUCCESS) {
            LOG_ERROR("mosquitto_loop (handshake driver) iter=%d failed: %d %s",
                      i, lr, mosquitto_strerror(lr));
            return THING_MODEL_RC_TRANSPORT_ERROR;
        }
    }
    if (t->connect_rc == -1) {
        LOG_ERROR("CONNACK timeout 5s — broker %s:%u did not reply. TLS handshake not completed "
                  "(see the [TLS] lines above) or the broker is not responding to MQTT CONNECT.",
                  cfg->host, cfg->port);
        return THING_MODEL_RC_TIMEOUT;
    }
    if (t->connect_rc != 0) {
        LOG_ERROR("CONNACK rc=%d (%s) — broker rejected (credential/ACL issue)",
                  t->connect_rc, mosquitto_connack_string(t->connect_rc));
        return THING_MODEL_RC_AUTH_FAILED;
    }

    /* CONNACK received, handshake complete → start the background thread to take over publish/subscribe */
    int sr = mosquitto_loop_start(t->mosq);
    if (sr != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("mosquitto_loop_start (post-CONNACK) failed: %d %s",
                  sr, mosquitto_strerror(sr));
        return THING_MODEL_RC_TRANSPORT_ERROR;
    }

    /* Auto-reconnect — while the libmosquitto loop thread is running it calls mosquitto_loop_misc
     * to detect disconnects, and auto-reconnects per the backoff strategy set here.
     * SUBSCRIBE is not auto-retained — the session layer maintains the subscription list and re-subscribes on on_connect. */
    if (cfg->auto_reconnect) {
        uint16_t min_s = cfg->reconnect_delay_min_sec ? cfg->reconnect_delay_min_sec : 1;
        uint16_t max_s = cfg->reconnect_delay_max_sec ? cfg->reconnect_delay_max_sec : 30;
        int dr = mosquitto_reconnect_delay_set(t->mosq, min_s, max_s, true /* exp backoff */);
        if (dr == MOSQ_ERR_SUCCESS) {
            LOG_INFO("[RECONNECT] auto-reconnect enabled (delay %u..%u s, exp backoff)",
                     min_s, max_s);
        } else {
            LOG_ERROR("mosquitto_reconnect_delay_set failed: %d %s",
                      dr, mosquitto_strerror(dr));
            /* Not fatal, continue — use libmosquitto's default reconnect strategy */
        }
    }

    /* In TLS mode: wait for ssl_info_cb to grab the SSL */
    if (cfg->use_tls) {
        for (int i = 0; i < 100 && t->current_ssl == NULL; i++) o->sleep_ms(10);
    }
    return THING_MODEL_RC_OK;
}

static T_ThingModelReturnCode m_disconnect(T_ThingModelTransportImpl_t *t)
{
    if (!t || !t->mosq) return THING_MODEL_RC_OK;
    mosquitto_disconnect(t->mosq);
    /* Graceful stop (force=false): mosquitto_disconnect() already tells the loop to exit, so we
     * just join it. Do NOT use force=true here — that pthread_cancel()s the network thread, which
     * can be cancelled at a write() cancellation point *inside* the logger (ssl_info_cb prints the
     * "close notify" line) while it holds the global log mutex, leaking the lock and deadlocking
     * the next log call (observed: intermittent hang right after Phase-1 "close notify"). */
    mosquitto_loop_stop(t->mosq, false);
    t->current_ssl = NULL;
    return THING_MODEL_RC_OK;
}

static T_ThingModelReturnCode m_publish(T_ThingModelTransportImpl_t *t, const char *topic,
                                 const void *payload, size_t len, int qos, bool retain)
{
    if (!t || !t->mosq || !topic) return THING_MODEL_RC_INVALID_ARG;

    if (t->hmac_enabled) {
        size_t cap = len + 40;
        uint8_t *buf = malloc(cap);
        if (!buf) return THING_MODEL_RC_OUT_OF_MEMORY;
        uint64_t ts = ThingModelOsal_Get()->time_ms();
        size_t out_len = 0;
        if (ThingModelSA_SignPayload(t->crypto, t->hmac_key, payload, len, ts, buf, cap, &out_len) != 0) {
            free(buf); return THING_MODEL_RC_UNKNOWN;
        }
        int rc = mosquitto_publish(t->mosq, NULL, topic, (int)out_len, buf, qos, retain);
        free(buf);
        if (rc != MOSQ_ERR_SUCCESS) {
            thing_model_record_error(THING_MODEL_RC_TRANSPORT_ERROR, rc, 0,
                                     "mosquitto_publish(hmac) topic=%s rc=%d %s",
                                     topic, rc, mosquitto_strerror(rc));
            return THING_MODEL_RC_TRANSPORT_ERROR;
        }
        return THING_MODEL_RC_OK;
    }

    int rc = mosquitto_publish(t->mosq, NULL, topic, (int)len, payload, qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        thing_model_record_error(THING_MODEL_RC_TRANSPORT_ERROR, rc, 0,
                                 "mosquitto_publish topic=%s rc=%d %s",
                                 topic, rc, mosquitto_strerror(rc));
        return THING_MODEL_RC_TRANSPORT_ERROR;
    }
    return THING_MODEL_RC_OK;
}

static T_ThingModelReturnCode m_subscribe(T_ThingModelTransportImpl_t *t, const char *filter, int qos)
{
    if (!t || !t->mosq) return THING_MODEL_RC_NOT_CONNECTED;
    int rc = mosquitto_subscribe(t->mosq, NULL, filter, qos);
    return (rc == MOSQ_ERR_SUCCESS) ? THING_MODEL_RC_OK : THING_MODEL_RC_TRANSPORT_ERROR;
}

static T_ThingModelReturnCode m_unsubscribe(T_ThingModelTransportImpl_t *t, const char *filter)
{
    if (!t || !t->mosq) return THING_MODEL_RC_NOT_CONNECTED;
    int rc = mosquitto_unsubscribe(t->mosq, NULL, filter);
    return (rc == MOSQ_ERR_SUCCESS) ? THING_MODEL_RC_OK : THING_MODEL_RC_TRANSPORT_ERROR;
}

static T_ThingModelReturnCode m_export_keying_material(T_ThingModelTransportImpl_t *t, uint8_t out[32])
{
    if (!t || !out) return THING_MODEL_RC_INVALID_ARG;
    if (!t->current_ssl) return THING_MODEL_RC_INVALID_ARG;
    /* RFC5705 TLS keying material export — bound to the current TLS connection,
     * an internal responsibility of the transport, not routed through the crypto vtable. */
    static const char SA_LABEL[] = "MQTT_SECURE_ACCESS_V1";
    int r = SSL_export_keying_material(t->current_ssl, out, 32,
                                       SA_LABEL, sizeof(SA_LABEL) - 1,
                                       NULL, 0, 0);
    return (r == 1) ? THING_MODEL_RC_OK : THING_MODEL_RC_AUTH_FAILED;
}

static const T_ThingModelTransportVtable MOSQ_VT = {
    .create                  = m_create,
    .destroy                 = m_destroy,
    .set_message_cb          = m_set_message_cb,
    .set_conn_cb             = m_set_conn_cb,
    .connect                 = m_connect,
    .disconnect              = m_disconnect,
    .publish                 = m_publish,
    .subscribe               = m_subscribe,
    .unsubscribe             = m_unsubscribe,
    .export_keying_material  = m_export_keying_material,
};

const T_ThingModelTransportVtable *ThingModelTransport_Mosquitto(void) { return &MOSQ_VT; }
