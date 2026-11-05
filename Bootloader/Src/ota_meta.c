/**
 * @file    ota_meta.c
 * @brief   元数据的读、写、校验
 *
 * 实现要点：
 * 1. 读：直接从 Flash 地址强转成指针读，不需要解锁
 * 2. 写：必须先擦除整页（1KB 或 2KB），再写
 * 3. 校验：magic 不对 或 crc_meta 不对，都算无效
 */
#include "ota_meta.h"
#include "crc32.h"
#include "stm32f1xx_hal.h"      /* 用到 HAL_FLASH_* 系列函数 */

#define META_CRC_LEN   (sizeof(ota_meta_t) - sizeof(uint32_t))  /* 前 20 字节 */

void ota_meta_load(ota_meta_t *m)
{
    const ota_meta_t *p = (const ota_meta_t *)OTA_META_ADDR;
    *m = *p;                    /* Flash 可以直接读，不需要解锁 */
}

int ota_meta_is_valid(const ota_meta_t *m)
{
    uint32_t crc;

    if (m->magic != OTA_META_MAGIC)   return 0;   /* 从没初始化过 */

    crc = crc32_calc((const uint8_t *)m, META_CRC_LEN);
    if (crc != m->crc_meta)           return 0;   /* 被写坏了 */

    if (m->active_slot > 1)           return 0;   /* 槽号非法 */
    return 1;
}

void ota_meta_set_default(ota_meta_t *m)
{
    m->magic       = OTA_META_MAGIC;
    m->active_slot = OTA_SLOT_A;    /* 首次上电默认启动 A */
    m->trial       = 0;
    m->boot_count  = 0;
    m->crc_a       = 0;
    m->crc_b       = 0;
    m->crc_meta    = crc32_calc((const uint8_t *)m, META_CRC_LEN);
}

void ota_meta_save(const ota_meta_t *m)
{
    /* TODO: 实现步骤
     * 1. HAL_FLASH_Unlock();
     * 2. 擦除 OTA_META_ADDR 所在的那一页（FLASH_EraseInitTypeDef）
     * 3. 按 32 位字（或半字）逐段写进去：HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data)
     * 4. HAL_FLASH_Lock();
     *
     * 注意：写之前要重算 crc_meta，别把旧的 CRC 写进去。
     */
}

uint32_t ota_slot_addr(uint8_t slot)
{
    return (slot == OTA_SLOT_A) ? OTA_SLOT_A_ADDR : OTA_SLOT_B_ADDR;
}
