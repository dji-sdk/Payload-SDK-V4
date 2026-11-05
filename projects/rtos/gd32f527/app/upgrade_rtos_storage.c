#include "upgrade_rtos_storage.h"

#include "flash_if.h"
#include "gd32f5xx.h"
#include "mbedtls/sha256.h"

#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define UPGRADE_FILE_INFO_ADDR (APPLICATION_STORE_ADDRESS_END - 1023U)
#define UPGRADE_IMAGE_MAX_SIZE (UPGRADE_FILE_INFO_ADDR - APPLICATION_STORE_ADDRESS)
#define UPGRADE_BOOT_MAGIC     0x11223344U
#define UPGRADE_COMPLETE_PENDING 0xFFFFFFFFU
#define UPGRADE_COMPLETE_FINISHED 0x00000000U

typedef struct {
    uint32_t file_size;
    char file_name[256];
    uint32_t completion_marker;
} UpgradeLoaderFileInfo;

typedef struct {
    uint32_t magic;
    uint32_t end_state;
    uint32_t image_size;
    uint8_t sha256[32];
} UpgradeBootRecord;

static struct {
    mbedtls_sha256_context sha;
    uint32_t expected_size;
    uint32_t received_size;
    uint8_t digest[32];
    bool receiving;
    bool ready;
    uint8_t sector_buf[4096];
    uint16_t sector_pos;
} s_upgrade;

static int image_vector_valid(uint32_t size)
{
    if (size < 8U) return 0;
    uint32_t sp = *(const uint32_t *)APPLICATION_STORE_ADDRESS;
    uint32_t pc = *(const uint32_t *)(APPLICATION_STORE_ADDRESS + 4U);
    return sp >= 0x20000000U && sp < 0x20100000U &&
           pc >= APPLICATION_ADDRESS && pc < (APPLICATION_ADDRESS_END + 1U);
}

int UpgradeRtosStorage_PreErase(void)
{
    printf("[UPG] pre-erase staging area\r\n");
    if (FLASH_If_Erase(APPLICATION_STORE_ADDRESS, APPLICATION_STORE_ADDRESS_END) != FLASHIF_OK) {
        printf("[UPG] staging pre-erase failed\r\n");
        return -1;
    }
    return 0;
}

int UpgradeRtosStorage_Begin(uint32_t expected_size)
{
    if (expected_size == 0U || expected_size > UPGRADE_IMAGE_MAX_SIZE) {
        printf("[UPG] reject image size=%lu max=%lu\r\n", (unsigned long)expected_size, (unsigned long)UPGRADE_IMAGE_MAX_SIZE);
        return -1;
    }
    memset(&s_upgrade, 0, sizeof(s_upgrade));
    mbedtls_sha256_init(&s_upgrade.sha);
    if (mbedtls_sha256_starts_ret(&s_upgrade.sha, 0) != 0) return -1;
    s_upgrade.expected_size = expected_size;
    s_upgrade.receiving = true;
    return 0;
}

int UpgradeRtosStorage_Append(const uint8_t *data, uint32_t len)
{
    if (!s_upgrade.receiving || !data || len == 0U ||
        len > s_upgrade.expected_size - s_upgrade.received_size) return -1;

    while (len > 0) {
        uint16_t space = sizeof(s_upgrade.sector_buf) - s_upgrade.sector_pos;
        uint16_t n = (len < space) ? (uint16_t)len : space;
        memcpy(s_upgrade.sector_buf + s_upgrade.sector_pos, data, n);
        s_upgrade.sector_pos += n;
        data += n;
        len -= n;

        if (s_upgrade.sector_pos == sizeof(s_upgrade.sector_buf)) {
            uint32_t address = APPLICATION_STORE_ADDRESS + s_upgrade.received_size;
            if (FLASH_If_Write(address, s_upgrade.sector_buf, s_upgrade.sector_pos) != FLASHIF_OK) {
                printf("[UPG] staging write failed offset=%lu\r\n", (unsigned long)s_upgrade.received_size);
                return -1;
            }
            if (mbedtls_sha256_update_ret(&s_upgrade.sha, s_upgrade.sector_buf, s_upgrade.sector_pos) != 0) return -1;
            s_upgrade.received_size += s_upgrade.sector_pos;
            if ((s_upgrade.received_size % (64U * 1024U)) < s_upgrade.sector_pos || s_upgrade.received_size == s_upgrade.expected_size)
                printf("[UPG] received %lu/%lu\r\n", (unsigned long)s_upgrade.received_size, (unsigned long)s_upgrade.expected_size);
            s_upgrade.sector_pos = 0;
        }
    }
    return 0;
}

int UpgradeRtosStorage_Finish(void)
{
    UpgradeLoaderFileInfo info;
    uint8_t read_digest[32];
    mbedtls_sha256_context verify;

    if (!s_upgrade.receiving || s_upgrade.received_size + s_upgrade.sector_pos != s_upgrade.expected_size) {
        printf("[UPG] image validation failed received=%lu buffered=%u expected=%lu\r\n",
               (unsigned long)s_upgrade.received_size, s_upgrade.sector_pos, (unsigned long)s_upgrade.expected_size);
        return -1;
    }

    if (s_upgrade.sector_pos > 0) {
        uint32_t address = APPLICATION_STORE_ADDRESS + s_upgrade.received_size;
        if (FLASH_If_Write(address, s_upgrade.sector_buf, s_upgrade.sector_pos) != FLASHIF_OK) return -1;
        if (mbedtls_sha256_update_ret(&s_upgrade.sha, s_upgrade.sector_buf, s_upgrade.sector_pos) != 0) return -1;
        s_upgrade.received_size += s_upgrade.sector_pos;
        s_upgrade.sector_pos = 0;
    }

    if (!image_vector_valid(s_upgrade.received_size)) {
        printf("[UPG] image vector invalid size=%lu\r\n", (unsigned long)s_upgrade.received_size);
        return -1;
    }
    if (mbedtls_sha256_finish_ret(&s_upgrade.sha, s_upgrade.digest) != 0) return -1;

    mbedtls_sha256_init(&verify);
    if (mbedtls_sha256_starts_ret(&verify, 0) != 0) return -1;
    for (uint32_t offset = 0; offset < s_upgrade.received_size; offset += 1024U) {
        uint32_t n = s_upgrade.received_size - offset;
        if (n > 1024U) n = 1024U;
        if (mbedtls_sha256_update_ret(&verify,
                                      (const uint8_t *)(APPLICATION_STORE_ADDRESS + offset), n) != 0) return -1;
    }
    if (mbedtls_sha256_finish_ret(&verify, read_digest) != 0 ||
        memcmp(s_upgrade.digest, read_digest, sizeof(read_digest)) != 0) return -1;

    memset(&info, 0, sizeof(info));
    info.file_size = s_upgrade.received_size;
    memcpy(info.file_name, "upgrade.bin", sizeof("upgrade.bin"));
    info.completion_marker = UPGRADE_COMPLETE_PENDING;
    if (FLASH_If_Write(UPGRADE_FILE_INFO_ADDR, (const uint8_t *)&info, sizeof(info)) != FLASHIF_OK) return -1;
    s_upgrade.receiving = false;
    s_upgrade.ready = true;
    printf("[UPG] staging SHA-256 verified, image ready\r\n");
    return 0;
}

bool UpgradeRtosStorage_IsReady(void) { return s_upgrade.ready; }
uint32_t UpgradeRtosStorage_Size(void) { return s_upgrade.received_size; }

int UpgradeRtosStorage_SetBootPending(void)
{
    UpgradeBootRecord record;
    if (!s_upgrade.ready) return -1;
    memset(&record, 0, sizeof(record));
    record.magic = UPGRADE_BOOT_MAGIC;
    record.end_state = 1U;
    record.image_size = s_upgrade.received_size;
    memcpy(record.sha256, s_upgrade.digest, sizeof(record.sha256));
    if (FLASH_If_Erase(APPLICATION_PARAM_STORE_ADDRESS, APPLICATION_PARAM_STORE_ADDRESS_END) != FLASHIF_OK) return -1;
    if (FLASH_If_Write(APPLICATION_PARAM_STORE_ADDRESS, (const uint8_t *)&record, sizeof(record)) != FLASHIF_OK) return -1;
    printf("[UPG] loader pending flag written\r\n");
    return 0;
}

bool UpgradeRtosStorage_ConsumeBootPending(void)
{
    const UpgradeBootRecord *record = (const UpgradeBootRecord *)APPLICATION_PARAM_STORE_ADDRESS;
    uint8_t digest[32];
    mbedtls_sha256_context sha;
    if (record->magic != UPGRADE_BOOT_MAGIC || record->end_state != 1U ||
        record->image_size == 0U || record->image_size > UPGRADE_IMAGE_MAX_SIZE) return false;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts_ret(&sha, 0) != 0) return false;
    for (uint32_t offset = 0; offset < record->image_size; offset += 1024U) {
        uint32_t n = record->image_size - offset;
        if (n > 1024U) n = 1024U;
        if (mbedtls_sha256_update_ret(&sha, (const uint8_t *)(APPLICATION_ADDRESS + offset), n) != 0) return false;
    }
    if (mbedtls_sha256_finish_ret(&sha, digest) != 0 ||
        memcmp(digest, record->sha256, sizeof(digest)) != 0) return false;
    return FLASH_If_Erase(APPLICATION_PARAM_STORE_ADDRESS, APPLICATION_PARAM_STORE_ADDRESS_END) == FLASHIF_OK;
}

static int hash_flash_region(uint32_t address, uint32_t size, uint8_t digest[32])
{
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    if (mbedtls_sha256_starts_ret(&sha, 0) != 0) return -1;
    for (uint32_t offset = 0; offset < size; offset += 1024U) {
        uint32_t n = size - offset;
        if (n > 1024U) n = 1024U;
        if (mbedtls_sha256_update_ret(&sha, (const uint8_t *)(address + offset), n) != 0) return -1;
    }
    return mbedtls_sha256_finish_ret(&sha, digest);
}

bool UpgradeRtosStorage_HasPendingCompletion(void)
{
    const UpgradeLoaderFileInfo *info = (const UpgradeLoaderFileInfo *)UPGRADE_FILE_INFO_ADDR;
    uint8_t staged_digest[32];
    uint8_t app_digest[32];

    if (info->completion_marker != UPGRADE_COMPLETE_PENDING ||
        info->file_size == 0U || info->file_size > UPGRADE_IMAGE_MAX_SIZE) return false;
    if (hash_flash_region(APPLICATION_STORE_ADDRESS, info->file_size, staged_digest) != 0 ||
        hash_flash_region(APPLICATION_ADDRESS, info->file_size, app_digest) != 0) return false;
    return memcmp(staged_digest, app_digest, sizeof(staged_digest)) == 0;
}

int UpgradeRtosStorage_MarkCompletionFinished(void)
{
    const uint32_t marker = UPGRADE_COMPLETE_FINISHED;
    return FLASH_If_Write(UPGRADE_FILE_INFO_ADDR + offsetof(UpgradeLoaderFileInfo, completion_marker),
                          (const uint8_t *)&marker, sizeof(marker)) == FLASHIF_OK ? 0 : -1;
}
