#ifndef __W25Q_H
#define __W25Q_H

#include <stdint.h>

uint32_t w25q_read_id(void);
void     w25q_read(uint32_t addr, uint8_t *buf, uint32_t len);
void     w25q_write(uint32_t addr, const uint8_t *buf, uint16_t len);
void     w25q_erase_sector(uint32_t addr);

#endif
