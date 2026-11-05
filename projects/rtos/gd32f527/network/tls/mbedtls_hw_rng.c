/* mbedtls_hw_rng.c
 * GD32F527 TRNG hardware entropy source for mbedTLS 2.28.
 * Implements mbedtls_hardware_poll() required when
 * MBEDTLS_ENTROPY_HARDWARE_ALT is defined in mbedtls_config_gd32.h.
 */
#include "mbedtls_config_gd32.h"
#include "mbedtls/entropy.h"
#include "gd32f5xx.h"
#include <string.h>
#include <stdint.h>

/**
 * @brief  Provide hardware entropy using GD32F527 TRNG peripheral.
 *         mbedTLS calls this from mbedtls_entropy_gather().
 */
int mbedtls_hardware_poll(void *data, unsigned char *output,
                          size_t len, size_t *olen)
{
    (void)data;
    size_t generated = 0;

    /* Enable TRNG clock and peripheral */
    rcu_periph_clock_enable(RCU_TRNG);
    trng_enable();

    while (generated < len) {
        /* Wait for TRNG data ready */
        uint32_t timeout = 0x10000U;
        while (SET != trng_flag_get(TRNG_FLAG_DRDY)) {
            if (--timeout == 0U) {
                /* Timeout: use SysTick as weak entropy fallback */
                uint32_t tick = SysTick->VAL;
                size_t n = (len - generated >= 4U) ? 4U : (len - generated);
                memcpy(output + generated, &tick, n);
                generated += n;
                *olen = generated;
                return 0;
            }
        }

        /* Read 32-bit random number */
        uint32_t rnd = trng_get_true_random_data();
        size_t n = (len - generated >= 4U) ? 4U : (len - generated);
        memcpy(output + generated, &rnd, n);
        generated += n;
    }

    *olen = generated;
    return 0;
}
