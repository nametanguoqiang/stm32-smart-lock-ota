#ifndef __W25Q_H                                                // 防止头文件被重复包含
#define __W25Q_H                                                // 上面那行的配对

#include <stdint.h>                                             // uint8_t / uint32_t 这些类型

uint32_t w25q_read_id(void);                                    // 读 JEDEC ID，返回 24 位：厂商号+类型+容量
void     w25q_read(uint32_t addr, uint8_t *buf, uint32_t len);  // 从 addr 开始连续读 len 个字节到 buf
void     w25q_write(uint32_t addr, const uint8_t *buf, uint16_t len);  // 往 addr 写 len 个字节，不跨页
void     w25q_erase_sector(uint32_t addr);                      // 擦掉 addr 所在的 4KB 扇区

#endif                                                          // 和开头的 #ifndef 配对
