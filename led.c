#include "led.h"

GPIO_TypeDef* GPIO_PORT[LEDn] = {
	GLED_GPIO_PORT,
	RLED_GPIO_PORT,
};
const uint16_t GPIO_PIN[LEDn] = {
	GLED_PIN,
	RLED_PIN,
};

/*
const uint32_t GPIO_CLK[LEDn] = {
	GLED_GPIO_CLK,
	RLED_GPIO_CLK,
};
*/

void M2C_LEDInit(Led_TypeDef Led)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* Enable the GPIO_LED Clock */
	//RCC_AHBPeriphClockCmd(GPIO_CLK[Led], ENABLE);

	/* Configure the GPIO_LED pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN[Led];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT_PP;
	GPIO_Init(GPIO_PORT[Led], &GPIO_InitStructure);
	GPIO_PORT[Led]->BSR = GPIO_PIN[Led];
}

void M2C_LEDOn(Led_TypeDef Led)
{
	GPIO_PORT[Led]->BSR = GPIO_PIN[Led];
}

void M2C_LEDOff(Led_TypeDef Led)
{
	GPIO_PORT[Led]->BRR = GPIO_PIN[Led];
}

void M2C_LEDToggle(Led_TypeDef Led)
{
	GPIO_PORT[Led]->ODR ^= GPIO_PIN[Led];
}

void M2C_LEDBlink(Led_TypeDef Led, uint8_t times)
{
	times *= 2;
	for (; times > 0; times--)
	{
		M2C_LEDToggle(Led);
		M2C_Delay(M2C_DELAY_SHORT);
	}
}

/* vim: set tabstop=4 autoindent shiftwidth=4 smartindent fdm=syntax: */
