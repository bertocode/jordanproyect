  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

/* Includes ------------------------------------------------------------------*/
#include "stm32w108xx.h"
#include "stm32w108xx_it.h"
#include "m2c_defs.h"
#include "led.h"

#define FLASH_START_ADDRESS ((uint32_t)0x08000000)

#if defined (STM32W108CZ)
  #define FLASH_PAGE_SIZE   ((uint16_t)0x800)
  #define FLASH_END_ADDR    ((uint32_t)0x08030000)
#elif defined(STM32W108CC)
  #define FLASH_PAGE_SIZE   ((uint16_t)0x800)
  #define FLASH_END_ADDR    ((uint32_t)0x08040000)
#else
  #define FLASH_PAGE_SIZE   ((uint16_t)0x400)
  #define FLASH_END_ADDR    ((uint32_t)0x08010000)
#endif

#define APPLICATION_ADDRESS ((uint32_t)FLASH_START_ADDRESS + APPLICATION_OFFSET)
#define APPLICATION_END_ADDRESS FLASH_END_ADDR - FLASH_PAGE_SIZE

#define BOOT_MODE_BL  0X00
#define BOOT_MODE_APP 0XFF

#define CONFIG_MASK_ADDRESS ((uint32_t)FLASH_END_ADDR - FLASH_PAGE_SIZE)
#define BOOT_MODE ((uint8_t)(((*((uint32_t*) CONFIG_MASK_ADDRESS)) & 0xFF000000) >> 24))


/* Exported types ------------------------------------------------------------*/
#define DATA_RECORD                         0
#define END_OF_FILE_RECORD                  1
#define EXTENDED_SEGMENT_ADDRESS_RECORD     2
#define START_SEGMENT_ADDRESS_RECORD        3
#define EXTENDED_LINEAR_ADDRESS_RECORD      4
#define START_LINEAR_ADDRESS_RECORD         5

typedef struct {
	uint8_t num_bytes;
	uint16_t offset;
	uint8_t type;
	uint8_t data[0x10];
	uint8_t checksum;
} HexLine;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
