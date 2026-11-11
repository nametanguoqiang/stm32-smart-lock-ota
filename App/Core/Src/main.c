/* USER CODE BEGIN Header */
/**
	******************************************************************************
	* @file           : main.c
	* @brief          : Main program body
	******************************************************************************
	* @attention
	*
	* Copyright (c) 2026 STMicroelectronics.
	* All rights reserved.
	*
	* This software is licensed under terms that can be found in the LICENSE file
	* in the root directory of this software component.
	* If no LICENSE file comes with this software, it is provided AS-IS.
	*
	******************************************************************************
	*/
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>      // printf 和 FILE 类型的声明
#include "w25q.h"       // w25q_read_id 的声明
#include "ota_meta.h"
#include "crc32.h"      // crc32_flash 的声明，算 A/B 区 CRC 用
#include "FreeRTOS.h"
#include "task.h"
#include "app_oled.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define FW_SIZE  16384                                      // 升级包大小，和 Bootloader 的 COPY_SIZE 一致

static int ota_copy_to_b(void)									// 从 W25Q 读升级包写进 B 区，0 成功
{
	static uint8_t buf[256];                                // 一页缓冲
	uint32_t off, i, bad = 0, err = 0;                      // 偏移、下标、错误计数、擦除错误页号
	FLASH_EraseInitTypeDef e;                               // 擦除参数结构体

	printf("[App] erase B...\r\n");                         // 开始擦 B 区

	HAL_FLASH_Unlock();                                     // 解锁 Flash 控制寄存器
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);  // 清残留错误标志

	e.TypeErase   = FLASH_TYPEERASE_PAGES;                  // 按页擦
	e.PageAddress = OTA_SLOT_B_ADDR;                        // B 区起始地址 0x0801C000
	e.NbPages     = FW_SIZE / 2048;                         // 16KB / 2KB = 8 页

	if (HAL_FLASHEx_Erase(&e, &err) != HAL_OK)              // 擦失败就退出
	{
		HAL_FLASH_Lock();                                   // 锁回去
		printf("[App] erase fail, page err = %u\r\n", err);  // 报错
		return -1;                                          // -1 表示擦除失败
	}

	printf("[App] copy W25Q -> B...\r\n");                  // 开始搬

	for (off = 0; off < FW_SIZE; off += 256)                // 16KB / 256 = 64 页
	{
		w25q_read(off, buf, 256);                           // 从 W25Q 读一页

		for (i = 0; i < 256; i += 4)                        // 一页拆成 64 个字
		{
			uint32_t w = (uint32_t)buf[i]                   // 小端拼接：低字节在前
				| ((uint32_t)buf[i + 1] << 8)               // 第 2 个字节左移 8 位
				| ((uint32_t)buf[i + 2] << 16)              // 第 3 个字节左移 16 位
				| ((uint32_t)buf[i + 3] << 24);             // 第 4 个字节左移 24 位

			if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,   // 一次写 32 位
					OTA_SLOT_B_ADDR + off + i,              // 写到 B 区的对应位置
					w) != HAL_OK)                           // 要写的内容
			{
				HAL_FLASH_Lock();                           // 锁回去
				printf("[App] write fail at %u\r\n", off + i);  // 报错
				return -2;                                  // -2 表示写入失败
			}
		}
	}

	HAL_FLASH_Lock();                                       // 写完上锁

	printf("[App] verify...\r\n");                          // 开始回读比对

	for (off = 0; off < FW_SIZE; off += 256)                // 再走一遍
	{
		w25q_read(off, buf, 256);                           // 从 W25Q 再读一页
		for (i = 0; i < 256; i++)                           // 逐字节比
			if (buf[i] != *(volatile uint8_t *)(OTA_SLOT_B_ADDR + off + i))	// 和 B 区的对应字节比
				bad++;                                      // 不一致就加一
	}

printf("[App] copy to B: %u bytes, bad %u\r\n", FW_SIZE, bad);  // 报结果

	if (bad != 0)                                                   // 比对发现有字节不一样
	{
		printf("[App] B corrupt, meta not saved\r\n");              // B 区不可信
		return -3;                                                  // 直接退出，元数据一个字不动
	}

	ota_meta_t meta;                                                // 用来装当前元数据
	ota_meta_load(&meta);                                           // 从 Flash 最后一页读出来

	if (!ota_meta_is_valid(&meta))                                  // 没初始化过，或者被写坏了
	{
		ota_meta_set_default(&meta);                                // 先填默认值，后面再覆盖字段
	}

	meta.crc_a       = crc32_flash(OTA_SLOT_A_ADDR, FW_SIZE);       // A 区这 16KB 的 CRC
	meta.crc_b       = crc32_flash(OTA_SLOT_B_ADDR, FW_SIZE);       // B 区这 16KB 的 CRC
	meta.trial       = 1;                                           // 标记成试启动，等 App 自检后再清
	meta.boot_count  = 0;                                           // 试启动失败次数从 0 开始
	meta.active_slot = OTA_SLOT_B;                                  // 下次上电启动 B 区
	meta.ota_req     = 0;                                           // 请求已经处理完，清掉
	ota_meta_save(&meta);                                           // 写回 Flash，save 里会重算 crc_meta

	printf("[App] meta: slot=%d trial=%d cnt=%d crc_a=0x%08X crc_b=0x%08X\r\n",  // 打印确认
		meta.active_slot, meta.trial, meta.boot_count,              // 槽号、试启动、失败次数
		meta.crc_a, meta.crc_b);                                    // 两个槽的 CRC

	return 0;                                                       // 成功
}

void vApplicationMallocFailedHook(void)                             // 堆不够时进来
{
	taskDISABLE_INTERRUPTS();                                       // 关中断，停在原地
	for (;;)
	{
	}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)  // 任务栈溢出时进来
{
	(void)xTask;
	(void)pcTaskName;
	taskDISABLE_INTERRUPTS();                                       // 关中断，停在原地
	for (;;)
	{
	}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */


	printf("[App] W25Q ID = 0x%06X\r\n", w25q_read_id());   // 读 JEDEC ID，验证 App 侧 SPI 通不通
	printf("[App] hello\r\n");                              // 确认 printf 本身能用
  
	printf("[App] main=0x%08X VTOR=0x%08X\r\n",                     // 本镜像的链接地址
  (uint32_t)&main, SCB->VTOR);                                // 和向量表基址

	ota_meta_t meta;                                                // 当前元数据
	uint8_t running_a;                                              // 1 = 现在跑在 A 区
	uint8_t b_active;                                               // 1 = B 是当前活动槽

	ota_meta_load(&meta);                                           // 从 Flash 读元数据
	running_a = (SCB->VTOR == OTA_SLOT_A_ADDR);                     // 向量表在 A 就说明跑在 A
	b_active  = ota_meta_is_valid(&meta)                            // 元数据可信
		&& (meta.active_slot == OTA_SLOT_B);                        // 而且目标是 B

	if (running_a && !b_active && meta.ota_req == 1)                // 跑在 A、B 不是目标、而且有升级请求
	{
		ota_copy_to_b();                                            // 才允许写 B 区
	}
	else                                                            // 其它情况
	{
		printf("[App] skip ota, vtor=0x%08X slot=%d req=%d\r\n",    // 不动 Flash
			SCB->VTOR, meta.active_slot, meta.ota_req);             // 打出来看原因
	}

	// ===== 试启动确认：能跑到这里，说明固件活到了 main =====
	if (ota_meta_is_valid(&meta) && meta.trial == 1)                // 这次是试启动
	{
		if (SCB->VTOR == ota_slot_addr(meta.active_slot))           // 跑的就是活动槽
		{
			meta.trial = 0;                                         // 确认能跑
			meta.boot_count = 0;                                    // 计数归零
			ota_meta_save(&meta);                                   // 写回
			printf("[App] confirmed, trial cleared\r\n");           // 打印
		}
		else                                                        // 回退上来的
		{
			printf("[App] fallback boot, not confirm\r\n");         // 不清 trial
		}
	}

	app_oled_start(&hi2c1);                                      // 初始化 OLED，建显示任务
	vTaskStartScheduler();                                       // 启动调度器，正常不返回
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


// 重定向 printf 到串口1
int fputc(int ch, FILE *f)                                          // printf 每输出一个字符就调它一次
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);   // 用串口1发这一个字节，死等到发完
	return ch;                                                      // 原样返回，不然 printf 以为写失败会中断
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
