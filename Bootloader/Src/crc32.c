#include "crc32.h"                                              // 本模块的函数声明

#define CRC32_POLY  0xEDB88320U                                 // 生成多项式的反射写法（0x04C11DB7 位反转）

uint32_t crc32_calc(const uint8_t *data, uint32_t len)          // 算 data 开始 len 个字节的 CRC32
{
    uint32_t crc = 0xFFFFFFFFU;                                 // 初值全 1
    uint32_t i;                                                 // 字节下标
    int b;                                                      // 位下标，0 到 7

    for (i = 0; i < len; i++)                                   // 逐字节处理
    {
        crc ^= data[i];                                         // 先把当前字节异或进低 8 位
        for (b = 0; b < 8; b++)                                 // 每个字节处理 8 位
        {
            if (crc & 1U)                                       // 最低位是 1
                crc = (crc >> 1) ^ CRC32_POLY;                  // 右移后异或多项式
            else                                                // 最低位是 0
                crc >>= 1;                                      // 只右移
        }
    }
    return ~crc;                                                // 最后整体取反
}

uint32_t crc32_flash(uint32_t addr, uint32_t len)               // 算 Flash 里 addr 开始 len 个字节的 CRC32
{
    // STM32 的 Flash 是内存映射的，0x08000000 开始的地址可以直接当指针读
    return crc32_calc((const uint8_t *)addr, len);              // 把地址强转成指针，交给 crc32_calc
}
