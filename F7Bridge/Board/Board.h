/**
******************************************************************************
* File Name          : Board.h
******************************************************************************
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOARD_H
#define __BOARD_H

#ifdef __cplusplus
 extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef STM32F7
#include "stm32f7xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h" 
#endif   
   
#include "usb_host.h"
#include "fatfs.h"  
#include "ftp_server.h"
#include "esp8266_io.h"
#include "SD.h"

#ifdef STM32F7
//#include "Clock_F7.h"
#elif defined(STM32F4)
//#include "Clock_F4.h"
#endif 


/* Exported types ------------------------------------------------------------*/
/*###############################################################*/
/*###############################################################* DEBUG -->*/
/*###############################################################*/
#define DEBUG_FTP
#define DEBUG_ESP8266
#define DEBUG_SD
/* Exported constants --------------------------------------------------------*/
/**
  * @brief  Terminal main structure
  */
typedef struct __BOARD_HandleTypeDef
{
	void(*init)(void);
} BOARD_HandleTypeDef;
/* Exported Macros -----------------------------------------------------------*/

_ARMABI int header_print(const char* str, const char* format, ...);
_ARMABI int print(const char *format, ...);
#define debug_header header_print
#define debug_print	printf

#define BOARD_STK_SZ (1024*2)
#define SD_STK_SZ (1024*2)
#define FTP_STK_SZ (1024*2)


#define member_size(type, member) sizeof(((type *)0)->member)
typedef struct
{
  uint8_t data[USART_BUFFER_SIZE];
  uint16_t tail;
  uint16_t head;
} RingBuffer_t;
/* Exported functions --------------------------------------------------------*/
extern void Error_Handler(void);
void Board_Init(void);
#ifdef __cplusplus
}
#endif

#endif /*__BOARD_H*/
