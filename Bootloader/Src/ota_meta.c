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
    ota_meta_t tmp = *m;                                                  // 先拷一份，不动调用者传进来的数据
    tmp.crc_meta = crc32_calc((const uint8_t *)&tmp, META_CRC_LEN);       // 按最终要写的内容重算自校验

    FLASH_EraseInitTypeDef erase;                                         // 擦除参数结构体
    uint32_t page_err = 0;                                                // 擦除失败时，这里回填出错的页号

    HAL_FLASH_Unlock();                                                   // 解锁 Flash 控制寄存器，不解锁写不进去

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);  // 清掉上一轮残留的错误标志

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;                            // 按页擦，不是整片擦
    erase.PageAddress = OTA_META_ADDR;                                    // 擦元数据所在那一页
    erase.NbPages     = 1;                                                // 只擦一页（2KB）

    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK)                   // 擦除失败就退出
    {
        HAL_FLASH_Lock();                                                 // 失败也要锁回去，不然 Flash 一直开着
        return;
    }

    const uint32_t *src = (const uint32_t *)&tmp;                         // 把结构体当成一串 32 位字
    for (uint32_t i = 0; i < sizeof(ota_meta_t) / 4; i++)                 // 20 字节 / 4 = 5 次
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,                     // 一次写 32 位
                                OTA_META_ADDR + i * 4,                      // 第 i 个字要写到哪个地址
                                src[i]) != HAL_OK)                          // 要写的内容
        {
            HAL_FLASH_Lock();                                             // 写失败就停，别继续写
            return;
        }
    }

    HAL_FLASH_Lock();                                                     // 全部写完，锁回去防止误写


}

uint32_t ota_slot_addr(uint8_t slot)
{
    return (slot == OTA_SLOT_A) ? OTA_SLOT_A_ADDR : OTA_SLOT_B_ADDR;
}
