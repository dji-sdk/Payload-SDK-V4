/**
 * @file    ca_cert_provider.c
 * @brief   Implements transport_lwip's weak functions, exposing the PEM array embedded by ca_cert.h to the runtime.
 *
 *   In transport_lwip.c:
 *     __attribute__((weak)) const unsigned char *thing_model_lwip_ca_cert(void) { return NULL; }
 *     __attribute__((weak)) unsigned int thing_model_lwip_ca_cert_len(void) { return 0; }
 *
 *   This file provides the strong definitions; the linker prefers this file's version.
 */
#include "ca_cert.h"   /* provides static const unsigned char s_ca_cert_pem[] */

const unsigned char *thing_model_lwip_ca_cert(void)
{
    return s_ca_cert_pem;
}

unsigned int thing_model_lwip_ca_cert_len(void)
{
    return sizeof(s_ca_cert_pem);
}
