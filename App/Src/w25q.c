#include "w25q.h"                                           // 本模块自己的函数声明
#include "stm32f1xx_hal.h"                                  // HAL 库：SPI 收发和 GPIO 操作

extern SPI_HandleTypeDef hspi2;                             // hspi2 定义在 main.c 里，这里声明才能用

#define W25Q_CS_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET)  // 片选拉低：选中芯片，开始一次通信
#define W25Q_CS_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET)  // 片选拉高：取消选中，芯片执行刚收到的指令

#define W25Q_CMD_WREN    0x06                               // 写使能：擦或写之前必须先发一次
#define W25Q_CMD_RDSR1   0x05                               // 读状态寄存器 1：看 bit0 忙不忙
#define W25Q_CMD_READ    0x03                               // 读数据：可以连续读任意长度
#define W25Q_CMD_PP      0x02                               // 页编程：往一页写最多 256 字节
#define W25Q_CMD_SE      0x20                               // 扇区擦除：擦掉一个 4KB
#define W25Q_CMD_JEDEC   0x9F                               // 读 JEDEC ID：拿厂商号+类型+容量

static uint8_t w25q_xfer(uint8_t d)                         // 发一个字节，同时收回一个字节
{
	uint8_t r = 0xFF;                                       // 默认按 MISO 空闲高电平填，失败时不返回栈垃圾
	if (HAL_SPI_TransmitReceive(&hspi2, &d, &r, 1, 100) != HAL_OK)  // SPI 全双工：发 d 收 r，失败返回非 HAL_OK
	{
		return 0xFF;                                        // 传输失败，返回空闲值，调用者按无效处理
	}
	return r;                                               // 传输成功，返回收到的字节
}

static void w25q_wren(void)                                 // 发一次写使能，相当于拿一张一次性的通行证
{
	W25Q_CS_LOW();                                          // 选中芯片
	w25q_xfer(W25Q_CMD_WREN);                               // 发 0x06
	W25Q_CS_HIGH();                                         // 拉高，芯片把写使能锁存位置 1
}

static void w25q_wait(void)                                 // 等芯片空闲，擦除和写入之后都要等
{
	W25Q_CS_LOW();                                          // 选中芯片
	w25q_xfer(W25Q_CMD_RDSR1);                              // 发 0x05，接下来读到的就是状态寄存器
	while (w25q_xfer(0xFF) & 0x01);                         // 一直读，bit0（BUSY）是 1 就继续等
	W25Q_CS_HIGH();                                         // 空闲了，取消选中
}

uint32_t w25q_read_id(void)                                 // 读 JEDEC ID，返回 24 位
{
	uint32_t id = 0;                                        // 结果先清零
	uint8_t retry;                                          // 重试次数

	for (retry = 0; retry < 3; retry++)                     // 最多试三次，排除偶发失败
	{
		id = 0;                                             // 每次重试都从零开始拼
		W25Q_CS_LOW();                                      // 选中芯片
		w25q_xfer(W25Q_CMD_JEDEC);                          // 发 0x9F
		id |= (uint32_t)w25q_xfer(0xFF) << 16;              // 第 1 个字节是厂商号，放最高 8 位
		id |= (uint32_t)w25q_xfer(0xFF) << 8;               // 第 2 个字节是类型，放中间 8 位
		id |= (uint32_t)w25q_xfer(0xFF);                    // 第 3 个字节是容量，放最低 8 位
		W25Q_CS_HIGH();                                     // 取消选中

		if (id != 0xFFFFFF && id != 0x000000)               // 不是全 1 也不是全 0，认为读到了
		{
			return id;                                      // 返回有效 ID
		}
	}

	return id;                                              // 三次都无效，返回最后一次的值
}

void w25q_read(uint32_t addr, uint8_t *buf, uint32_t len)   // 从 addr 开始连续读 len 个字节到 buf
{
	W25Q_CS_LOW();                                          // 选中芯片
	w25q_xfer(W25Q_CMD_READ);                               // 发 0x03
	w25q_xfer(addr >> 16);                                  // 地址高字节（24 位地址分三次发）
	w25q_xfer(addr >> 8);                                   // 地址中字节
	w25q_xfer(addr);                                        // 地址低字节
	for (uint32_t i = 0; i < len; i++)                      // 循环 len 次
		buf[i] = w25q_xfer(0xFF);                           // 发 0xFF 产生时钟，收回来的就是数据
	W25Q_CS_HIGH();                                         // 读完了，取消选中
}

void w25q_write(uint32_t addr, const uint8_t *buf, uint16_t len)  // 往 addr 写 len 个字节，不跨页
{
	w25q_wren();                                            // 先拿通行证，不发写不进去
	W25Q_CS_LOW();                                          // 选中芯片
	w25q_xfer(W25Q_CMD_PP);                                 // 发 0x02
	w25q_xfer(addr >> 16);                                  // 地址高字节
	w25q_xfer(addr >> 8);                                   // 地址中字节
	w25q_xfer(addr);                                        // 地址低字节
	for (uint16_t i = 0; i < len; i++)                      // 循环 len 次
		w25q_xfer(buf[i]);                                  // 一个字节一个字节推出去
	W25Q_CS_HIGH();                                         // 拉高，芯片开始真正写
	w25q_wait();                                            // 等写完，一页约 0.7ms
}

void w25q_erase_sector(uint32_t addr)                       // 擦掉 addr 所在的 4KB 扇区
{
	w25q_wren();                                            // 先拿通行证
	W25Q_CS_LOW();                                          // 选中芯片
	w25q_xfer(W25Q_CMD_SE);                                 // 发 0x20
	w25q_xfer(addr >> 16);                                  // 地址高字节
	w25q_xfer(addr >> 8);                                   // 地址中字节
	w25q_xfer(addr);                                        // 地址低字节
	W25Q_CS_HIGH();                                         // 拉高，芯片开始真正擦
	w25q_wait();                                            // 等擦完，一个扇区约 45ms
}
