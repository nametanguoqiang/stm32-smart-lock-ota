#include "w25q.h"
#include "stm32f1xx_hal.h"

extern SPI_HandleTypeDef hspi2;

#define W25Q_CS_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)
#define W25Q_CS_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)

#define W25Q_CMD_WREN    0x06
#define W25Q_CMD_RDSR1   0x05
#define W25Q_CMD_READ    0x03
#define W25Q_CMD_PP      0x02
#define W25Q_CMD_SE      0x20
#define W25Q_CMD_JEDEC   0x9F

static uint8_t w25q_xfer(uint8_t d)
{
    uint8_t r;
    HAL_SPI_TransmitReceive(&hspi2, &d, &r, 1, 100);
    return r;
}

static void w25q_wren(void)
{
    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_WREN);
    W25Q_CS_HIGH();
}

static void w25q_wait(void)
{
    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_RDSR1);
    while (w25q_xfer(0xFF) & 0x01);
    W25Q_CS_HIGH();
}

uint32_t w25q_read_id(void)
{
    uint32_t id = 0;

    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_JEDEC);
    id |= (uint32_t)w25q_xfer(0xFF) << 16;
    id |= (uint32_t)w25q_xfer(0xFF) << 8;
    id |= (uint32_t)w25q_xfer(0xFF);
    W25Q_CS_HIGH();

    return id;
}

void w25q_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_READ);
    w25q_xfer(addr >> 16);
    w25q_xfer(addr >> 8);
    w25q_xfer(addr);
    for (uint32_t i = 0; i < len; i++)
        buf[i] = w25q_xfer(0xFF);
    W25Q_CS_HIGH();
}

void w25q_write(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    w25q_wren();
    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_PP);
    w25q_xfer(addr >> 16);
    w25q_xfer(addr >> 8);
    w25q_xfer(addr);
    for (uint16_t i = 0; i < len; i++)
        w25q_xfer(buf[i]);
    W25Q_CS_HIGH();
    w25q_wait();
}

void w25q_erase_sector(uint32_t addr)
{
    w25q_wren();
    W25Q_CS_LOW();
    w25q_xfer(W25Q_CMD_SE);
    w25q_xfer(addr >> 16);
    w25q_xfer(addr >> 8);
    w25q_xfer(addr);
    W25Q_CS_HIGH();
    w25q_wait();
}
