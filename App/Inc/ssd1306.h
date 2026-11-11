#ifndef __SSD1306_H
#define __SSD1306_H

#include "main.h"

#define SSD1306_I2C_ADDR   0x78
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES      8

void ssd1306_init(I2C_HandleTypeDef *hi2c);
void ssd1306_clear(void);
void ssd1306_draw_char(uint8_t x, uint8_t page, char c);
void ssd1306_draw_string(uint8_t x, uint8_t page, const char *s);
void ssd1306_refresh(void);

#endif
