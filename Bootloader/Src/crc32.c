/**
 * @file    crc32.c
 * @brief   CRC32 实现
 *
 * 用逐位算法而不是查表：慢一点，但不用占 1KB 的常量表，
 * Bootloader 只有 16KB，省着点用。
 */
#include "crc32.h"

#define CRC32_POLY  0xEDB88320U

uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;      /* 初值全 1 */
    uint32_t i;
    int b;

    for (i = 0; i < len; i++) {
        crc ^= data[i];              /* 先把当前字节异或进低 8 位 */
        for (b = 0; b < 8; b++) {
            if (crc & 1U)            /* 最低位是 1 就右移后异或多项式 */
                crc = (crc >> 1) ^ CRC32_POLY;
            else                     /* 最低位是 0 就只右移 */
                crc >>= 1;
        }
    }
    return ~crc;                     /* 最后取反 */
}

uint32_t crc32_flash(uint32_t addr, uint32_t len)
{
    /* STM32 的 Flash 是内存映射的，0x08000000 开始的地址可以直接当指针读 */
    return crc32_calc((const uint8_t *)addr, len);
}
