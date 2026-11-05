/**
 * @file    ota.c
 * @brief   App 侧升级实现
 *
 * 完整流程：
 *   收到固件 → 写非活动槽 → 边写边算 CRC → 校验 → 改元数据 → 复位
 */
#include "ota.h"
#include "ota_meta.h"
#include "crc32.h"
#include "stm32f1xx_hal.h"

void ota_confirm_boot(void)
{
    /* TODO:
     * 1. 读元数据
     * 2. 如果 trial == 1，说明这次是新固件第一次启动
     * 3. 自检通过（屏幕正常、指纹模块正常、Flash 可读）后：
     *      meta.trial = 0;
     *      meta.boot_count = 0;
     *      ota_meta_save(&meta);
     */
}

int ota_write_firmware(const uint8_t *buf, uint32_t len)
{
    /* TODO:
     * 1. 读元数据，算出「非活动槽」的地址
     * 2. 解锁 Flash
     * 3. 逐页擦除目标槽（注意：擦除单位是页，不是字节）
     * 4. 按字写入
     * 5. 边写边算 CRC
     * 6. 写完回读校验
     * 7. 校验通过 → 更新 meta.crc_x
     * 8. 上锁
     */
    (void)buf; (void)len;
    return OTA_ERR_FLASH;
}

void ota_switch_and_reset(void)
{
    /* TODO:
     * 1. 读元数据
     * 2. active_slot 取反
     * 3. trial = 1，boot_count = 0
     * 4. ota_meta_save()
     * 5. NVIC_SystemReset()
     */
}
