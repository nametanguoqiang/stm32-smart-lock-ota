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
#include "crc32.h"      // crc32_calc 的声明
#include "ota_meta.h"   // ota_meta_t 类型和那五个函数的声明
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
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//
//修改VTOR寄存器的值，指向App的中断向量表
typedef void (*app_entry_t)(void);                        // 函数指针类型：无参数、无返回值
static void jump_to_app(uint32_t app_addr)                // 传入 App 分区起始地址
{
    uint32_t sp = *(volatile uint32_t *)app_addr;         // 向量表第 0 项，是 App 的栈顶地址
    uint32_t pc = *(volatile uint32_t *)(app_addr + 4);   // 第 1 项，是 Reset_Handler 的地址

    if ((sp & 0xFFF00000) != 0x20000000) return;          // 栈顶必须落在 SRAM（0x20000000 起 48KB），不对就说明这没固件

    __disable_irq();                                      // 关总中断，防止换栈换表的过程被打断
    SysTick->CTRL = 0;                                    // 关 SysTick，不然跳过去它还在数，一溢出就跳回 Bootloader
    SysTick->LOAD = 0;                                    // 清装载值
    SysTick->VAL  = 0;                                    // 清当前计数
    SCB->VTOR = app_addr;                                 // 把向量表基址指到 App 的表
    __set_MSP(sp);                                        // 把主栈指针换成 App 的栈顶
    __enable_irq();                                       // 重新开中断，不开的话 App 里所有中断都不响应
    ((app_entry_t)pc)();                                  // 跳到 App 的 Reset_Handler，不返回
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  //ota测试代码
  ota_meta_t meta;                                                             // 用来装读出来的元数据
  ota_meta_load(&meta);                                                        // 从 Flash 最后一页读出来

  printf("magic=0x%08X slot=%d trial=%d cnt=%d\r\n",                            // 把关键字段打出来
  meta.magic, meta.active_slot, meta.trial, meta.boot_count);
  if (!ota_meta_is_valid(&meta))                                               // magic 或 crc_meta 不对
  {
      printf("meta invalid, init default\r\n");                                // 说明是第一次上电
      ota_meta_set_default(&meta);    

      ota_meta_save(&meta);                                                    // 写回 Flash
      printf("saved: slot=%d\r\n", meta.active_slot);                          // 确认写的是 A
  }
  else
  {
      printf("meta ok\r\n");                                                   // 元数据有效，直接用
  }


  //CRC32 的标准测试
  const char *vec = "123456789";                        // CRC32 的标准测试向量
  uint32_t got = crc32_calc((const uint8_t *)vec, 9);    // 算这 9 个字节
  printf("CRC32 = 0x%08X\r\n", got);                     // 打印结果
  printf("expect= 0xCBF43926\r\n");                      // 打印期望值，肉眼比


  //亮灯判断是否进入Bootloader段
  for (int i = 0; i < 3; i++)                                        // 快闪三下，和 App 的慢闪区分开
  {
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);         // 拉低，灯亮
      HAL_Delay(100);                                                // 100 毫秒
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);           // 拉高，灯灭
      HAL_Delay(100);                                                // 100 毫秒
  }

jump_to_app(0x08004000);                                           // 交给 App

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
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);

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
