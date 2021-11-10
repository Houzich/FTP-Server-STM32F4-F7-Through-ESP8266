/**
  ******************************************************************************
  * @file    esp8266_io.h
  * @author  
  * @brief   
  ******************************************************************************
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ESP8266_IO_H
#define __ESP8266_IO_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#ifdef STM32F7
#include "stm32f7xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h" 
#endif   
#include "cmsis_os.h"
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define DEFAULT_TIME_OUT                 3000 /* in ms */

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
int8_t ESP8266_IO_Init(void);
void ESP8266_IO_DeInit(void);

int8_t ESP8266_IO_Send(uint8_t* Buffer, uint32_t Length);
int32_t ESP8266_IO_Receive(uint8_t* Buffer, uint32_t Length);
int32_t ESP8266_IO_Read(uint8_t *Buffer, int *num_pct, int max_len);
void ESP8266Task(void const * argument);
extern osThreadId ESP8266TaskHandle;
#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_IO_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
