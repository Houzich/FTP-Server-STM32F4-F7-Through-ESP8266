/**
  ******************************************************************************
  * File Name          : SD.c
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "cmsis_os.h"
#include "Board.h"
#include "ff.h"  
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
//used when debugging, declared in the file Street_Clock.h
#ifdef DEBUG_SD
#undef DEBUG /* DEBUG */
#undef DEBUG_PRINT /* DEBUG_PRINT */
#define DEBUG(...)		do {debug_header("SD: ", __VA_ARGS__);} while (0)
#define DEBUG_PRINT(x)		debug_print x;
#else
#define DEBUG(...)
#define DEBUG_PRINT(...)
#endif // DEBUG_TERMINAL_SETTINGS

extern void MX_USB_HOST_Init(void);
static void MSC_Application(void);

SD_HandleTypeDef sd = {
	Board_Init,
	.sd_ready = false,
};

/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
osThreadId sdTaskHandle;
osMessageQId SDEvent;
 //Mutex that protects critical sections
osMutexDef (SDMutex);  // Mutex name definition 
osMutexId SDMutex_id;  // Mutex id populated by the function CreateMutex()
/*###############################################################*/
/*###############################################################* SD_Init -->*/
/*###############################################################*/
void SD_Init(void)
{
	sd.sd_ready = false;
	osMessageQDef(osqueue, 10, uint16_t);
  SDEvent = osMessageCreate(osMessageQ(osqueue), NULL);
	
	SDMutex_id = osMutexCreate  (osMutex (SDMutex));
  if (SDMutex_id != NULL)  {
    // Mutex object created
  } 
	
	MX_USB_HOST_Init();
	
  osThreadDef(sdTask, SD_Thread, osPriorityNormal, 0, SD_STK_SZ);
  sdTaskHandle = osThreadCreate(osThread(sdTask), NULL);
}
/*###############################################################*/
/*###############################################################* SD_Init -->*/
/*###############################################################*/
__NO_RETURN void SD_Thread (void const * argument) {
  /* init code for USB_HOST */
  osEvent event;
	DEBUG("Start SD_Thread\n");
  /* Link the USB Host disk I/O driver */
	

    for( ;; )
    {
      event = osMessageGet(SDEvent, osWaitForever);

      if(event.status == osEventMessage)
      {
        switch(event.value.v)
        {
        case CONNECTION_EVENT:
					DEBUG("CONNECTION_EVENT\n");
          MSC_Application();
          break;

        case DISCONNECTION_EVENT:
					DEBUG("DISCONNECTION_EVENT\n");
          f_mount(NULL, (TCHAR const*)"", 0);
					sd.sd_ready = false;
          break;

        default:
          break;
        }
      }
    }
}
/**
 * Gets a string in 10.23.12.98 format and returns a four byte IP address array
 *
 * @param string containing IP address
 * @param numerical representation of IP address
 * @return 1 for OK, otherwise failed
 */
uint8_t
ip_convert_address (char *ipstring, char ipaddr[])
{
    int i,j,k,l;

    l = strlen(ipstring);

    for (i = 0; i < l; i++)
    {
        if ((ipstring[i] != '.') && ((ipstring[i] < '0') ||
            (ipstring[i] > '9')))
        {
            return 0;
        }
    }
    /* the characters in the string are at least valid */
    j = 0;
    k = 0;
    for (i = 0; i < l; i++)
    {
        if (ipstring[i] != '.')
        {
            if  (++j > 3)   /* number of digits in portion */
                return 0;
        }
        else
        {
            if (++k > 3)    /* number of periods */
                return 0;

            if (j == 0)     /* number of digits in portion */
                return 0;
            else
                j = 0;
        }
    }

    /* at this point, all the pieces are good, form ip_address */
    k = 0; j = 0;
    for (i = 0; i < l; i++)
    {
        if (ipstring[i] != '.')
        {
            k = (k * 10) + ipstring[i] - '0';
        }
        else
        {
            ipaddr[j++] = (uint8_t)k;
            k = 0;
        }
    }
    ipaddr[j] = (uint8_t)k;
    return 1;
}
/*###############################################################*/
/*###############################################################* MSC_Application -->*/
/*###############################################################*/
/**
  * @brief  Main routine for Mass Storage Class.
  * @param  None
  * @retval None
  */
	char ipstr[50] = {0};
	char gwstr[50] = {0};
	char netmaskstr[50] = {0};
	char userstr[50] = {0};
	char passwordstr[50] = {0};
	uint8_t ip[4];
	uint8_t gw[4];
	uint8_t netmask[4];
	uint8_t filetext[300];
	uint8_t temptext[300];
static void MSC_Application(void)
{
  FRESULT res;                                          /* FatFs function common result code */
  //uint32_t byteswritten, bytesread;                     /* File write/read counts */
  //uint8_t wtext[] = "This is STM32 working with FatFs"; /* File write buffer */



	uint32_t bytesread; 	/* File read buffer */
	if (FATFS_LinkDriver(&USBH_Driver, USBHPath) != FR_OK)
	{
		Error_Handler();
	}
  /* Register the file system object to the FatFs module */
  if(f_mount(&USBHFatFS, (TCHAR const*)USBHPath, 0) != FR_OK)
  {
    /* FatFs Initialization Error */
    Error_Handler();
  }
  else
  {		
		
		res = f_chdrive(USBHPath);
		if(res != FR_OK)Error_Handler();
		//for(;;)
		//{
			if(fsFileExists("WIFI.txt")){
				if(fsOpenFile(&USBHFile, "WIFI.txt", FA_READ) == FR_OK){
					 memset(filetext,0,sizeof(filetext));
						memset(temptext,0,sizeof(filetext));
					 if(fsReadFile(&USBHFile, filetext, 300, &bytesread) == FR_OK){
						 strncpy((void *)temptext, (void *)filetext, sizeof(temptext) - 1);
						 for(int i = 0; i< sizeof(temptext); i++){
							 if((temptext[i] == '\n')||(temptext[i] == '\r'))temptext[i] ='\0';
						 }
						 char *ptr = (void *)temptext;
						 memset(ipstr,0,sizeof(ipstr));
						 memset(gwstr,0,sizeof(gwstr));						 
						 memset(netmaskstr,0,sizeof(netmaskstr));						 
						 memset(userstr,0,sizeof(userstr));						 
						 memset(passwordstr,0,sizeof(passwordstr));						 
						 
						 strncpy(ipstr, ptr, sizeof(ipstr) - 1);
						 ptr += strlen(ipstr) + 2;
						 
						 strncpy(gwstr, ptr, sizeof(gwstr) - 1);
						 ptr += strlen(gwstr) + 2;

						 strncpy(netmaskstr, ptr, sizeof(netmaskstr) - 1);
						 ptr += strlen(netmaskstr) + 2;

						 strncpy(userstr, ptr, sizeof(userstr) - 1);
						 ptr += strlen(userstr) + 2;

						 strncpy(passwordstr, ptr, sizeof(passwordstr) - 1);
						 ptr += strlen(passwordstr) + 2;

						ip_convert_address(ipstr, (void *)ip);
						ip_convert_address(gwstr, (void *)gw);
						ip_convert_address(netmaskstr, (void *)netmask);
						sd.sd_ready = true;
					 }
				}
	
			}			
		//}
		
		
		
		

		//res = f_getcwd((void *)rtext, 300);
		//if(res != FR_OK)Error_Handler();
		//scan_files((void *)rtext);
		
		
//		DEBUG("Create and Open STM32.TXT\n");
//    /* Create and Open a new text file object with write access */
//    if(f_open(&USBHFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
//    {
//      /* 'STM32.TXT' file Open for write Error */
//      Error_Handler();
//    }
//    else
//    {
//      /* Write data to the text file */
//      res = f_write(&USBHFile, wtext, sizeof(wtext), (void *)&byteswritten);
//			DEBUG("Write data to the STM32.TXT\n");
//			DEBUG("DATA: %s\n", (char *)&byteswritten);
//      if((byteswritten == 0) || (res != FR_OK))
//      {
//        /* 'STM32.TXT' file Write or EOF Error */
//        Error_Handler();
//      }
//      else
//      {
//        /* Close the open text file */
//        f_close(&USBHFile);
//				DEBUG("Close the open STM32.TXT\n");
//        /* Open the text file object with read access */
//        if(f_open(&USBHFile, "STM32.TXT", FA_READ) != FR_OK)
//        {
//          /* 'STM32.TXT' file Open for read Error */
//          Error_Handler();
//        }
//        else
//        {
//          /* Read data from the text file */
//          res = f_read(&USBHFile, rtext, sizeof(rtext), (void *)&bytesread);
//					DEBUG("Read data from the STM32.TXT\n");
//          if((bytesread == 0) || (res != FR_OK))
//          {
//            /* 'STM32.TXT' file Read or EOF Error */
//            Error_Handler();
//          }
//          else
//          {
//						DEBUG("DATA: %s\n", (char *)&bytesread);
//            /* Close the open text file */
//            f_close(&USBHFile);

//            /* Compare read data with the expected data */
//            if((bytesread != byteswritten))
//            {
//              /* Read data is different from the expected data */
//              Error_Handler();
//            }
//            else
//            {
//              /* Success of the demo: no error occurrence */
//							DEBUG("Compare read data with the expected data\n");
//							DEBUG("Success!: no error occurrence\n");
//              //BSP_LED_On(LED1);
//            }
//          }
//        }
//      }
//    }
  }
	
//	if (f_mount(NULL, "", 0) != FR_OK)
//	{
//		DEBUG("ERROR : Cannot DeInitialize FatFs! \n");
//	}
//  /* Unlink the USB disk I/O driver */
//  if (FATFS_UnLinkDriver(USBHPath) != 0)
//	{
//		DEBUG("ERROR : Cannot UnLink FatFS Driver! \n");
//	};
}



FRESULT SD_getcwd(char *buff)
{
	FRESULT res;
	res = f_getcwd((void *)buff, 300);
	if(res != FR_OK)Error_Handler();
	return res;
}


FRESULT scan_files (
    char* path        /* Start node to be scanned (***also used as work area***) */
)
{
    FRESULT res;
    DIR dir;
    //UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
//                i = strlen(path);
//                sprintf(&path[i], "/%s", fno.fname);
//                res = scan_files(path);                    /* Enter the directory */
//                if (res != FR_OK) break;
//                path[i] = 0;
            } else {                                       /* It is a file. */
                //DEBUG("%s/%s\n", path, fno.fname);
								
            }
						DEBUG("%s\n", fno.fname);
        }
        f_closedir(&dir);
    }

    return res;
}


  
 /**
  * @brief Check whether a file exists
  * @param[in] path NULL-terminated string specifying the filename
  * @return The function returns TRUE if the file exists. Otherwise FALSE is returned
  **/

 bool fsFileExists(const char *path)
 {
    FRESULT res;
    FILINFO fno;

	  //fno.fname = NULL;
    fno.fsize = 0;
	 
    //Make sure the pathname is valid
    if(path == NULL)
       return false;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);
  
    //Check whether the file exists
    res = f_stat(path, &fno);

    //Leave critical section
    osMutexRelease(SDMutex_id);
 
    //Any error to report?
    if(res != FR_OK)
       return false;
  
    //Valid file?
    if(fno.fattrib & AM_DIR)
       return false;
    else
       return true;
 }
  
  
 /**
  * @brief Retrieve the size of the specified file
  * @param[in] path NULL-terminated string specifying the filename
  * @param[out] size Size of the file in bytes
  * @return Error code
  **/
  
 FRESULT fsGetFileSize(const char *path, uint32_t *size)
 {
    FRESULT res;
    FILINFO fno;

    //fno.fname = NULL;
    fno.fsize = 0;
	 
    //Check parameters
    if(path == NULL || size == NULL)
       return FR_INVALID_PARAMETER;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);

  
    //Retrieve information about the specified file
    res = f_stat(path, &fno);
  
    //Leave critical section
    osMutexRelease(SDMutex_id);

  
    //Any error to report?
    if(res != FR_OK)
       return res;
  
    //Valid file?
    if(fno.fattrib & AM_DIR)
       return FR_INVALID_PARAMETER;
  
    //Return the size of the file
    *size = fno.fsize;
  
    //Successful processing
    return FR_OK;
 }
 

 
  /**
  * @brief Write data to the specified file
  * @param[in] file Handle that identifies the file to be written
  * @param[in] data Pointer to a buffer containing the data to be written
  * @param[in] length Number of data bytes to write
  * @return Error code
  **/
  
 FRESULT fsWriteFile(FIL *file, void *data, size_t length, size_t *numwrbytes)
 {
    UINT n;
    FRESULT res;
  
    //Make sure the file pointer is valid
    if(file == NULL)
       return FR_INVALID_PARAMETER;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);
  
    //Write data
    res = f_write(file, data, length, numwrbytes);
  
    //Leave critical section
    osMutexRelease(SDMutex_id);
  
    //Any error to report?
    if(res != FR_OK)
       return res;
  
    //Sanity check
    if(*numwrbytes != length)
       return FR_DISK_ERR;
  
    //Successful processing
    return FR_OK;
 }
  
  /**
  * @brief Delete a file
  * @param[in] path NULL-terminated string specifying the filename
  * @return Error code
  **/
  
 FRESULT fsDeleteFile(const char *path)
 {
    FRESULT res;
  
    //Make sure the pathname is valid
    if(path == NULL)
       return FR_INVALID_PARAMETER;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);
  
    //Delete the specified file
    res = f_unlink(path);
  
    //Leave critical section
    osMutexRelease(SDMutex_id);
  
    //Any error to report?
    if(res != FR_OK)
       return res;
  
    //Successful processing
    return FR_OK;
 }
 /**
  * @brief Open the specified file for reading or writing
  * @param[in] path NULL-terminated string specifying the filename
  * @param[in] mode Type of access permitted (FS_FILE_MODE_READ,
  *   FS_FILE_MODE_WRITE or FS_FILE_MODE_CREATE)
  * @return File handle
  **/
  
FRESULT fsOpenFile( FIL *file, const char *path, uint32_t flags)
 {
    uint32_t i;
    FRESULT res;
  
    //Make sure the pathname is valid
    if(path == NULL)
       return NULL;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);
  
		//Open the specified file
		res = f_open(file, path, flags);
 
    //Leave critical section
    osMutexRelease(SDMutex_id);
    //Return a handle to the file
    return res;
 }
  /**
  * @brief Read data from the specified file
  * @param[in] file Handle that identifies the file to be read
  * @param[in] data Pointer to the buffer where to copy the data
  * @param[in] size Size of the buffer, in bytes
  * @param[out] length Number of data bytes that have been read
  * @return Error code
  **/
  
 FRESULT fsReadFile(FIL *file, void *data, size_t size, size_t *length)
 {
    UINT n;
    FRESULT res;
  
    //Check parameters
    if(file == NULL || length == NULL)
       return FR_INVALID_PARAMETER;
  
    //No data has been read yet
    *length = 0;
  
    //Enter critical section
    osMutexWait(SDMutex_id,osWaitForever);
  
    //Read data
    res = f_read((FIL *) file, data, size, &n);
  
    //Leave critical section
    osMutexRelease(SDMutex_id);
  
    //Any error to report?
    if(res != FR_OK)
       return res;
  
    //End of file?
    if(!n)
       return FR_INVALID_OBJECT;
  
    //Total number of data that have been read
    *length = n;
    //Successful processing
    return FR_OK;
 }
 
 
