#ifndef	__LED_H
#define __LED_H

#include "stm32w108xx.h"
#include "m2c_defs.h"

/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
typedef enum 
{
	GLED = 0,
	RLED = 1,
} Led_TypeDef;

/* Private define ------------------------------------------------------------*/
#define LEDn	2

#define GLED_PIN                         GPIO_Pin_6
#define GLED_GPIO_PORT                   GPIOA
#define GLED_GPIO_CLK                    RCC_AHBPeriph_GPIOA

#define RLED_PIN                         GPIO_Pin_7
#define RLED_GPIO_PORT                   GPIOA
#define RLED_GPIO_CLK                    RCC_AHBPeriph_GPIOA


/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void M2C_LEDInit(Led_TypeDef Led);
void M2C_LEDOn(Led_TypeDef Led);
void M2C_LEDOff(Led_TypeDef Led);
void M2C_LEDToggle(Led_TypeDef Led);
void M2C_LEDBlink(Led_TypeDef Led, uint8_t times);


#endif
/* vim: set tabstop=4 autoindent shiftwidth=4 smartindent fdm=syntax: */
