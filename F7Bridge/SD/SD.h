/**
******************************************************************************
* File Name          : SD.h
******************************************************************************
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SD_H
#define __SD_H

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
 
#include "fatfs.h"  
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/**
  * @brief  Terminal main structure
  */
typedef struct __SD_HandleTypeDef
{
	void(*init)(void);
	bool sd_ready;
} SD_HandleTypeDef;
/* Exported Macros -----------------------------------------------------------*/
   
/* Exported functions --------------------------------------------------------*/
extern void Error_Handler(void);
extern osMessageQId SDEvent;
extern SD_HandleTypeDef sd;
extern uint8_t filetext[300];

void SD_Init(void);
FRESULT SD_getcwd(char *buff);
__NO_RETURN void SD_Thread (void const * argument);
	FRESULT scan_files ( char* path );
	FRESULT fsGetFileSize(const char *path, uint32_t *size);
	bool fsFileExists(const char *path);
	FRESULT fsWriteFile(FIL *file, void *data, size_t length, size_t *numwrbytes);
 FRESULT fsDeleteFile(const char *path);
 FRESULT fsReadFile(FIL *file, void *data, size_t size, size_t *length);
FRESULT fsOpenFile( FIL *file, const char *path, uint32_t flags);
#ifdef __cplusplus
}
#endif

#endif /*__SD_H*/
