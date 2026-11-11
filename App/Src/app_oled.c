#include "app_oled.h"
#include "ssd1306.h"
#include "ota_meta.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

static void vOledTask(void *pvParameters)
{
	(void)pvParameters;
	char line[24];

	for (;;)
	{
		ssd1306_clear();
		ssd1306_draw_string(0, 0, "SmartLock");

		if (SCB->VTOR == OTA_SLOT_A_ADDR)
		{
			ssd1306_draw_string(0, 2, "OTA slot: A");
		}
		else
		{
			ssd1306_draw_string(0, 2, "OTA slot: B");
		}

		snprintf(line, sizeof(line), "tick: %lu", (unsigned long)xTaskGetTickCount());
		ssd1306_draw_string(0, 4, line);

		ssd1306_refresh();
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void app_oled_start(I2C_HandleTypeDef *hi2c)
{
	ssd1306_init(hi2c);

	if (HAL_I2C_IsDeviceReady(hi2c, SSD1306_I2C_ADDR, 3, 100) != HAL_OK)
	{
		printf("[OLED] not found, check 0x78 / PB6 / PB7\r\n");
	}

	xTaskCreate(vOledTask, "OLED", 256, NULL, 1, NULL);
}
