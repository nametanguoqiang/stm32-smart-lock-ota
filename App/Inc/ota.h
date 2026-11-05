/**
 * @file    ota.h
 * @brief   App 侧的升级接口
 *
 * App 要做两件事：
 *   1. 启动后确认自己活着（清掉试启动标记）
 *   2. 收到固件后写进另一个槽，切标记，复位
 */
#ifndef __OTA_H
#define __OTA_H

#include <stdint.h>

#define OTA_OK          0
#define OTA_ERR_PARAM  -1
#define OTA_ERR_CRC    -2
#define OTA_ERR_FLASH  -3

/* 启动后调用一次：自检通过就清掉试启动标记 */
void ota_confirm_boot(void);

/* 把 buffer 里的固件写到「非活动槽」，成功返回 OTA_OK */
int ota_write_firmware(const uint8_t *buf, uint32_t len);

/* 切换活动槽并复位（下次启动就跑新固件） */
void ota_switch_and_reset(void);

#endif
