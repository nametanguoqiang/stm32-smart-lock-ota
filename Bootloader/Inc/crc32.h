#ifndef __CRC32_H                                               // 防止头文件被重复包含
#define __CRC32_H                                               // 上面那行的配对

#include <stdint.h>                                             // uint8_t / uint32_t 这些类型

uint32_t crc32_calc(const uint8_t *data, uint32_t len);         // 算一段内存的 CRC32
uint32_t crc32_flash(uint32_t addr, uint32_t len);              // 算一段 Flash 的 CRC32

#endif                                                          // 和开头的 #ifndef 配对
