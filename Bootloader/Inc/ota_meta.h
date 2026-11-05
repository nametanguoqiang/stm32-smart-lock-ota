/**
 * @file    ota_meta.h
 * @brief   元数据读写：记录「当前该启动哪个槽」「是否处于试启动」
 *
 * 元数据是整个 OTA 的"记忆"，掉电后靠它决定跳哪个 App。
 * 它自己也可能被写坏，所以带 magic 和 crc_meta 双重校验。
 */
#ifndef __OTA_META_H
#define __OTA_META_H

#include <stdint.h>

/* ---------- 地址定义（必须和 App 侧、链接脚本保持一致） ---------- */
#define OTA_BOOT_ADDR      0x08000000U
#define OTA_SLOT_A_ADDR    0x08004000U
#define OTA_SLOT_B_ADDR    0x0801C000U
#define OTA_SLOT_SIZE      0x18000U        /* 96KB */
#define OTA_META_ADDR      0x08034000U     /* 元数据所在页 */
#define OTA_META_MAGIC     0x5A5AA5A5U

/* ---------- 槽编号 ---------- */
#define OTA_SLOT_A         0
#define OTA_SLOT_B         1

/* ---------- 元数据结构 ---------- */
typedef struct {
    uint32_t magic;         /* 固定 0x5A5AA5A5，判断这块区域有没有被初始化过 */
    uint8_t  active_slot;   /* 当前该启动哪个槽：0=A，1=B */
    uint8_t  trial;         /* 1 = 新固件处于试启动，还没被 App 确认 */
    uint16_t boot_count;    /* 试启动状态下连续启动了几次 */
    uint32_t crc_a;         /* App-A 的 CRC32 */
    uint32_t crc_b;         /* App-B 的 CRC32 */
    uint32_t crc_meta;      /* 本结构体前 20 字节的 CRC32 */
} ota_meta_t;

/* ---------- 接口 ---------- */
void ota_meta_load(ota_meta_t *m);          /* 从 Flash 读出来 */
void ota_meta_save(const ota_meta_t *m);    /* 写回 Flash（先擦后写） */
int  ota_meta_is_valid(const ota_meta_t *m);/* magic 和 crc_meta 都对才返回 1 */
void ota_meta_set_default(ota_meta_t *m);   /* 首次上电时初始化为「启动 A」 */
uint32_t ota_slot_addr(uint8_t slot);       /* 槽号转起始地址 */

#endif /* __OTA_META_H */
