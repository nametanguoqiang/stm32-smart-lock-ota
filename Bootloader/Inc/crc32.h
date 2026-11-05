/**
 * @file    crc32.h
 * @brief   CRC32 校验（多项式 0xEDB88320，即标准 CRC-32/ISO-HDLC）
 */
#ifndef __CRC32_H
#define __CRC32_H

#include <stdint.h>

uint32_t crc32_calc(const uint8_t *data, uint32_t len);          /* 算一段内存 */
uint32_t crc32_flash(uint32_t addr, uint32_t len);               /* 算一段 Flash */

#endif
