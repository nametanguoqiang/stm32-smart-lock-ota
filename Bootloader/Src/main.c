/**
 * @file    main.c
 * @brief   Bootloader 主流程
 *
 * 上电后做的事：
 *   ① 读元数据 → 无效就用默认值
 *   ② 判断 trial 状态，决定启动哪个槽
 *   ③ 校验目标槽的 CRC
 *   ④ 跳转
 */
#include "stm32f1xx_hal.h"
#include "ota_meta.h"
#include "crc32.h"

#define MAX_TRIAL_BOOT  3       /* 试启动最多允许失败几次 */

static void jump_to_app(uint32_t app_addr);

int main(void)
{
    HAL_Init();                 /* HAL 库初始化 */
    /* TODO: 初始化系统时钟（可以先用默认的 8MHz 内部时钟） */
    /* TODO: 初始化串口，用来打印调试信息 */

    ota_meta_t meta;
    ota_meta_load(&meta);

    /* ① 元数据无效 → 初始化成默认值（启动 A） */
    if (!ota_meta_is_valid(&meta)) {
        ota_meta_set_default(&meta);
        ota_meta_save(&meta);
    }

    /* ② 处理试启动 */
    if (meta.trial) {
        if (meta.boot_count >= MAX_TRIAL_BOOT) {
            /* 连续启动失败，回滚到另一个槽 */
            meta.active_slot ^= 1;
            meta.trial      = 0;
            meta.boot_count = 0;
            ota_meta_save(&meta);
        } else {
            meta.boot_count++;
            ota_meta_save(&meta);
        }
    }

    /* ③ 校验目标槽 */
    uint32_t addr = ota_slot_addr(meta.active_slot);
    uint32_t crc  = crc32_flash(addr, OTA_SLOT_SIZE);
    uint32_t expect = (meta.active_slot == OTA_SLOT_A) ? meta.crc_a : meta.crc_b;

    if (crc != expect) {
        /* 当前槽坏了，试试另一个 */
        uint8_t other = meta.active_slot ^ 1;
        uint32_t addr2 = ota_slot_addr(other);
        uint32_t crc2  = crc32_flash(addr2, OTA_SLOT_SIZE);
        uint32_t expect2 = (other == OTA_SLOT_A) ? meta.crc_a : meta.crc_b;

        if (crc2 == expect2) {
            meta.active_slot = other;
            ota_meta_save(&meta);
            addr = addr2;
        } else {
            /* 两个槽都坏了，只能停在这里等升级 */
            while (1) { /* TODO: 打印 "no valid app" 并等待串口升级 */ }
        }
    }

    /* ④ 跳转 */
    jump_to_app(addr);

    while (1) { }               /* 正常不会到这里 */
}

/**
 * @brief 跳到 App
 *
 * 四步缺一不可，漏一步就 HardFault：
 *   关中断 → 设 MSP → 设 VTOR → 跳到 Reset_Handler
 */
static void jump_to_app(uint32_t app_addr)
{
    typedef void (*app_entry_t)(void);
    app_entry_t entry;

    /* TODO: 1. 关掉所有中断（__disable_irq() 或者逐个关外设中断） */
    /* TODO: 2. 把 app_addr 处的第一个字（栈顶地址）写进 MSP */
    /*           __set_MSP(*(volatile uint32_t *)app_addr); */
    /* TODO: 3. 设向量表偏移：SCB->VTOR = app_addr; */
    /* TODO: 4. 取第二个字（Reset_Handler 地址）并调用 */

    entry = (app_entry_t)(*(volatile uint32_t *)(app_addr + 4));
    entry();
}
