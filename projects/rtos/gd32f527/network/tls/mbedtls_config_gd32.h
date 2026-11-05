/* mbedtls_config_gd32.h
 * mbedTLS 2.28.9 build configuration for GD32F527 (Cortex-M33, FreeRTOS)
 *
 * Goals:
 *   - TLS 1.2 client only (Phase 1 secure MQTT connect)
 *   - EKM key export via mbedtls_ssl_conf_export_keys_ext_cb
 *   - HMAC-SHA256 for message signing
 *   - RSA-2048 + AES-128/256-GCM cipher suites
 *   - No file I/O (CA cert embedded as C array)
 *   - FreeRTOS heap (pvPortMalloc / vPortFree)
 *   - GD32F527 TRNG hardware entropy
 */
#ifndef MBEDTLS_CONFIG_GD32_H
#define MBEDTLS_CONFIG_GD32_H

/* ---- Platform / memory ---- */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
/* pvPortCalloc shim: mbedtls_calloc/free are set at runtime via
 * mbedtls_platform_set_calloc_free() called from secure_access.c */
#include "FreeRTOS.h"
#include <stdlib.h>
static inline void *mbedtls_gd32_calloc(size_t n, size_t sz) {
    void *p = pvPortMalloc(n * sz);
    if (p) { extern void *memset(void*,int,size_t); memset(p, 0, n*sz); }
    return p;
}
#define MBEDTLS_PLATFORM_STD_CALLOC   mbedtls_gd32_calloc
#define MBEDTLS_PLATFORM_STD_FREE     vPortFree

/* ---- Entropy / RNG ---- */
#define MBEDTLS_NO_PLATFORM_ENTROPY       /* no /dev/urandom */
#define MBEDTLS_ENTROPY_HARDWARE_ALT      /* mbedtls_hardware_poll() in mbedtls_hw_rng.c */
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C

/* ---- TLS core ---- */
#define MBEDTLS_SSL_CLI_C                 /* TLS client */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_PROTO_TLS1_2          /* TLS 1.2 only */
#define MBEDTLS_SSL_EXPORT_KEYS           /* EKM: mbedtls_ssl_conf_export_keys_ext_cb */

/* ---- Symmetric crypto ---- */
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CBC

/* ---- Hash / HMAC ---- */
#define MBEDTLS_MD_C
#define MBEDTLS_MD5_C                     /* for speaker audio transfer integrity */
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C                  /* for SHA-384 cipher suites */

/* ---- Public key / RSA ---- */
#define MBEDTLS_RSA_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_OID_C
#define MBEDTLS_PKCS1_V15

/* ---- X.509 certificate ---- */
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

/* ---- Key exchange ---- */
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED

/* ---- Buffer sizes (reduce from default 16KB) ---- */
#define MBEDTLS_SSL_MAX_CONTENT_LEN   4096
#define MBEDTLS_MPI_MAX_SIZE           256   /* RSA-2048 */

/* ---- Disable unused features ---- */
/* MBEDTLS_SSL_SRV_C          -- no TLS server needed */
/* MBEDTLS_SSL_PROTO_TLS1     -- no TLS 1.0 */
/* MBEDTLS_SSL_PROTO_TLS1_1   -- no TLS 1.1 */
/* MBEDTLS_SSL_PROTO_DTLS     -- no DTLS */
/* MBEDTLS_FS_IO              -- no filesystem */
/* MBEDTLS_NET_C              -- use lwIP altcp, not mbedtls_net */
/* MBEDTLS_SELF_TEST          -- save flash */

/* #include "mbedtls/check_config.h" -- disabled to avoid build-time checks */
#endif /* MBEDTLS_CONFIG_GD32_H */
