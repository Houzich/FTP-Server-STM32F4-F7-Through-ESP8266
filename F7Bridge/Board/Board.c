/**
  ******************************************************************************
  * File Name          : Board.c
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cmsis_os.h"
#include "Board.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
//used when debugging, declared in the file Street_Clock.h
#ifdef DEBUG_CORE
#undef DEBUG /* DEBUG */
#undef DEBUG_PRINT /* DEBUG_PRINT */
#define DEBUG(...)		do {debug_header("CORE: ", __VA_ARGS__);} while (0)
#define DEBUG_PRINT(x)		debug_print x;
#else
#define DEBUG(...)
#define DEBUG_PRINT(...)
#endif // DEBUG_TERMINAL_SETTINGS

BOARD_HandleTypeDef streetclock = {
	Board_Init,
};

/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/*###############################################################*/
/*###############################################################* Street_Clock_Init -->*/
/*###############################################################*/
void Board_Init(void)
{
	SD_Init();
	ESP8266_IO_Init();
	FTP_Init();
}
/*###############################################################*/
/*###############################################################* Street_Clock_Thread -->*/
/*###############################################################*/
__NO_RETURN void Board_Thread (void *argument) {

	while (1) {
    osDelay(1); 
      
	}
}

_ARMABI int header_print(const char* str, const char* format, ...) {
	int ret;
	ret = printf("%s", str);
	va_list arg;
	va_start(arg, format);
	ret = vprintf(format, arg);
	va_end(arg);
	return ret;
}

_ARMABI int print(const char* format, ...) {
	int ret;
	va_list arg;
	va_start(arg, format);
	ret = vprintf(format, arg);
	va_end(arg);
	return ret;
}




