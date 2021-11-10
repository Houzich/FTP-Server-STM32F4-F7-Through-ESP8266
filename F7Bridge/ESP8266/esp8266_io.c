/**
  ******************************************************************************
  * @file    esp8266_io.c
  * @author  
  * @brief   
  ******************************************************************************
  * @attention

  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "fatfs.h"
#include "usart.h"
#include "Board.h"

#ifdef DEBUG_ESP8266
#undef DEBUG       /* DEBUG */
#undef DEBUG_PRINT /* DEBUG_PRINT */
#define DEBUG(...)                            \
  do                                          \
  {                                           \
    debug_header("STM USART: ", __VA_ARGS__); \
  } while (0)
#define DEBUG_PRINT(x) debug_print x;
#else
#define DEBUG(...)
#define DEBUG_PRINT(...)
#endif // DEBUG_ESP8266

#ifdef STM32F7
extern UART_HandleTypeDef huart6;
#define WiFiUartHandle huart6
#elif defined(STM32F4)
extern UART_HandleTypeDef huart2;
#define WiFiUartHandle huart2
#endif

/* Private define ------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
RingBuffer_t WiFiRxBuffer;
osThreadId ESP8266TaskHandle;

/* Private function prototypes -----------------------------------------------*/
static void WIFI_Handler(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  ESP8266 IO Initalization.
  *         This function inits the UART interface to deal with the esp8266,
  *         then starts asynchronous listening on the RX port.
  * @param None
  * @retval 0 on success, -1 otherwise.
  */
int8_t ESP8266_IO_Init(void)
{
  //  /* Set the WiFi USART configuration parameters */
  //  WiFiUartHandle.Instance        = USART6;
  //  WiFiUartHandle.Init.BaudRate   = 74880;
  //  WiFiUartHandle.Init.WordLength = UART_WORDLENGTH_8B;
  //  WiFiUartHandle.Init.StopBits   = UART_STOPBITS_1;
  //  WiFiUartHandle.Init.Parity     = UART_PARITY_NONE;
  //  WiFiUartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
  //  WiFiUartHandle.Init.Mode       = UART_MODE_TX_RX;
  //  WiFiUartHandle.Init.OverSampling = UART_OVERSAMPLING_16;

  //  /* Configure the USART IP */
  //  if(HAL_UART_Init(&WiFiUartHandle) != HAL_OK)
  //  {
  //    return -1;
  //  }

  /* Once the WiFi UART is intialized, start an asynchrounous recursive 
   listening. the HAL_UART_Receive_IT() call below will wait until one char is
   received to trigger the HAL_UART_RxCpltCallback(). The latter will recursively
   call the former to read another char.  */
  WiFiRxBuffer.head = 0;
  WiFiRxBuffer.tail = 0;
  HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
  HAL_UART_Receive_IT(&WiFiUartHandle, (uint8_t *)&WiFiRxBuffer.data[WiFiRxBuffer.tail], 1);

  //osThreadDef(esp8266Task, ESP8266Task, osPriorityNormal, 0, 4096);
  //ESP8266TaskHandle = osThreadCreate(osThread(esp8266Task), NULL);

  return 0;
}

/**
  * @brief  ESP8266 IO Deinitialization.
  *         This function Deinits the UART interface of the ESP8266. When called
  *         the esp8266 commands can't be executed anymore.
  * @param  None.
  * @retval None.
  */
void ESP8266_IO_DeInit(void)
{
  /* Reset USART configuration to default */
  HAL_UART_DeInit(&WiFiUartHandle);
}

/**
  * @brief  Send Data to the ESP8266 module over the UART interface.
  *         This function allows sending data to the  ESP8266 WiFi Module, the
  *          data can be either an AT command or raw data to send over 
             a pre-established WiFi connection.
  * @param pData: data to send.
  * @param Length: the data length.
  * @retval 0 on success, -1 otherwise.
  */
int8_t ESP8266_IO_Send(uint8_t *pData, uint32_t Length)
{
  ESPPacket_Typedef *packet;
  packet = (ESPPacket_Typedef *)pData;

  while (HAL_GPIO_ReadPin(COM_Input_GPIO_Port, COM_Input_Pin) == GPIO_PIN_RESET)
    osDelay(0);

  int len = Length;
  int frame_size;

  do
  {
    if (len > 120)
    {
      if (len == 121)
      {
        frame_size = 121;
      }
      else
      {
        frame_size = 120;
      }
    }
    else
    {
      frame_size = len;
    }
    len -= frame_size;
		osDelay(1);
    /* Unlike the ESP8266_IO_Receive(), the ESP8266_IO_Send() is using a blocking call
			 to ensure that the AT commands were correctly sent. */
    if (HAL_UART_Transmit(&WiFiUartHandle, (uint8_t *)pData, frame_size, DEFAULT_TIME_OUT) != HAL_OK)
    {
      return -1;
    }
    if ((len == 0) && (frame_size == 120))
    {
			osDelay(1);
      if (HAL_UART_Transmit(&WiFiUartHandle, (void *)"\0", 1, DEFAULT_TIME_OUT) != HAL_OK)
      {
        return -1;
      }
    }

    pData += frame_size;
  } while (len > 0);

  if (packet->header.id == ESP_FTP_COMMAND_ID)
    DEBUG("Send COMMAND: %d bytes\n%s\n", Length, packet->data);
  else if (packet->header.id == ESP_FTP_DATA_ID){
		if(Length < 100)
		    DEBUG("Send DATA: %d bytes\n%s\n", Length, packet->data);
		else{
			DEBUG("Send DATA: %d bytes\n", Length);
		}
	}

  else if (packet->header.id == ESP_LOG_ID)
    DEBUG("Send LOG: %d bytes\n%s\n", Length, packet->data);
  else
    DEBUG("Send Fail packet!!! %d bytes\n%s\n", Length, packet->data);
  return 0;
}

/**
  * @brief  Receive Data from the ESP8266 module over the UART interface.
  *         This function receives data from the  ESP8266 WiFi module, the
  *         data is fetched from a ring buffer that is asynchonously and continuously
            filled with the received data.
  * @param  Buffer: a buffer inside which the data will be read.
  * @param  Length: the Maximum size of the data to receive.
  * @retval int32_t: the actual data size that has been received.
  */
int32_t ESP8266_IO_Receive(uint8_t *Buffer, uint32_t Length)
{
  uint32_t ReadData = 0;

  /* Loop until data received */
  while (Length--)
  {
    uint32_t tickStart = HAL_GetTick();
    do
    {
      if (WiFiRxBuffer.head != WiFiRxBuffer.tail)
      {
        /* serial data available, so return data to user */
        *Buffer++ = WiFiRxBuffer.data[WiFiRxBuffer.head++];
        ReadData++;

        /* check for ring buffer wrap */
        if (WiFiRxBuffer.head >= member_size(RingBuffer_t, data))
        {
          /* Ring buffer wrap, so reset head pointer to start of buffer */
          WiFiRxBuffer.head = 0;
        }
        break;
      }
    } while ((HAL_GetTick() - tickStart) < DEFAULT_TIME_OUT);
  }

  return ReadData;
}

int32_t Check_Num_Packets(uint8_t *pBuffer, int32_t lenght)
{
  int num = 0;
  int buff_size = lenght;
  uint8_t *pBuff = pBuffer;
  ESPPacket_Typedef *pct;
  do
  {
    pct = (ESPPacket_Typedef *)pBuff;
    if ((pct->header.id != ESP_FTP_COMMAND_ID) && (pct->header.id != ESP_FTP_DATA_ID) && (pct->header.id != ESP_LOG_ID))
    {
      DEBUG("Eror Format Receive Packet %d bytes\n", lenght);
      for (int i = 0; i < lenght; i++)
        if (pBuffer[i] == 0)
          pBuffer[i] = '.';
      pct = (ESPPacket_Typedef *)pBuffer;
      //DEBUG("%s\n", pct->data);
      num = 0;
      break;
    }
    int packet_full_size = pct->header.len_data + sizeof(ESPHeadr_Typedef);
    if (packet_full_size <= buff_size)
    {
      buff_size -= packet_full_size;
      pct = (ESPPacket_Typedef *)pBuff;
      pReceivePackets[num] = (void *)pct;
      pBuff += packet_full_size;
      num++;
    }
    else
    {
      DEBUG("Eror Format Receive Packet. size packet &d > receive data %d bytes\n", packet_full_size, lenght);
      for (int i = 0; i < lenght; i++)
        if (pBuffer[i] == 0)
          pBuffer[i] = '.';
      pct = (ESPPacket_Typedef *)pBuffer;
      //DEBUG("%s\n", pct->data);
      num = 0;
      break;
    }
  } while (buff_size > 0);

  return num;
}

int32_t ESP8266_IO_Read(uint8_t *Buffer, int *num_pct, int max_len)
{
  int32_t ReadData = 0;
  uint32_t time_out = 0;
  /* Loop until data received */
  time_out = 0xFFFFFFFF;
  static ESPPacket_Typedef *packet;
  packet = (ESPPacket_Typedef *)Buffer;
	
  uint32_t tickStart = HAL_GetTick();
	if(WiFiRxBuffer.head == WiFiRxBuffer.tail)HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
  do
  {
    if (WiFiRxBuffer.head != WiFiRxBuffer.tail)
    {
      //if receiving data
      tickStart = HAL_GetTick();
      time_out = 2;

      /* serial data available, so return data to user */
      *Buffer++ = WiFiRxBuffer.data[WiFiRxBuffer.head++];
      ReadData++;

      /* check for ring buffer wrap */
      if (WiFiRxBuffer.head >= member_size(RingBuffer_t, data))
      {
        /* Ring buffer wrap, so reset head pointer to start of buffer */
        WiFiRxBuffer.head = 0;
      }
			if(ReadData == max_len) break;
    }
  } while ((HAL_GetTick() - tickStart) < time_out);

  if ((packet->header.id == ESP_FTP_COMMAND_ID) || (packet->header.id == ESP_FTP_DATA_ID) || (packet->header.id == ESP_LOG_ID))
  {
    //if (packet->header.len_data != ReadData - member_size(ESPPacket_Typedef, header))
    //{
    *num_pct = Check_Num_Packets((void *)packet, ReadData);
    //}
  }

  int i = 0;
  if (*num_pct > 0)
  {
    do
    {
      if (pReceivePackets[i]->header.id == ESP_FTP_COMMAND_ID)
        DEBUG("Read COMMAND %d bytes\n%s\n", ReadData, pReceivePackets[i]->data);
      else if (pReceivePackets[i]->header.id == ESP_FTP_DATA_ID)
        DEBUG("Read DATA %d bytes\n", ReadData);
      else if (pReceivePackets[i]->header.id == ESP_LOG_ID)
        DEBUG("Read ESP LOG %d bytes\n%s\n", ReadData, pReceivePackets[i]->data);
      else
        DEBUG("Read ESP BOOTLOADER %d bytes\n%s\n", ReadData, packet->data);
      i++;
    } while (i < *num_pct);
  }
  else
  {
    DEBUG("Read ESP BOOTLOADER %d bytes\n%s\n", ReadData, packet->data);
    HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
  }
  return ReadData;
}
/**
  * @brief  Rx Callback when new data is received on the UART.
  * @param  UartHandle: Uart handle receiving the data.
  * @retval None.
  */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
  /* If ring buffer end is reached reset tail pointer to start of buffer */
  if (++WiFiRxBuffer.tail >= member_size(RingBuffer_t, data))
  {
    WiFiRxBuffer.tail = 0;
  }
  HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_RESET);
  HAL_UART_Receive_IT(UartHandle, (uint8_t *)&WiFiRxBuffer.data[WiFiRxBuffer.tail], 1);
}

/**
  * @brief  Function called when error happens on the UART.
  * @param  UartHandle: Uart handle receiving the data.
  * @retval None.
  */

//  *            @arg @ref UART_FLAG_RWU   Receiver wake up flag (if the UART in mute mode)
//  *            @arg @ref UART_FLAG_SBKF  Send Break flag
//  *            @arg @ref UART_FLAG_CMF   Character match flag
//  *            @arg @ref UART_FLAG_BUSY  Busy flag
//  *            @arg @ref UART_FLAG_ABRF  Auto Baud rate detection flag
//  *            @arg @ref UART_FLAG_ABRE  Auto Baud rate detection error flag
//  *            @arg @ref UART_FLAG_CTS   CTS Change flag
//  *            @arg @ref UART_FLAG_LBDF  LIN Break detection flag
//  *            @arg @ref UART_FLAG_TXE   Transmit data register empty flag
//  *            @arg @ref UART_FLAG_TC    Transmission Complete flag
//  *            @arg @ref UART_FLAG_RXNE  Receive data register not empty flag
//  *            @arg @ref UART_FLAG_RTOF  Receiver Timeout flag
//  *            @arg @ref UART_FLAG_IDLE  Idle Line detection flag
//  *            @arg @ref UART_FLAG_ORE   Overrun Error flag
//  *            @arg @ref UART_FLAG_NE    Noise Error flag
//  *            @arg @ref UART_FLAG_FE    Framing Error flag
//  *            @arg @ref UART_FLAG_PE    Parity Error flag

void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
{
  // Dummy data read to clear the FE, PE, SR flag
  //UartHandle->Instance->DR;

  __HAL_UART_CLEAR_OREFLAG(UartHandle);
  __HAL_UART_CLEAR_NEFLAG(UartHandle);
  __HAL_UART_CLEAR_FEFLAG(UartHandle);
  __HAL_UART_CLEAR_PEFLAG(UartHandle);
  /* Call  the WIFI_Handler() to deinitialize the UART Interface. */
  //assert_param(1);
  //WIFI_Handler();
}

/**
  * @brief  Handler to deinialize the ESP8266 UART interface in case of errors.
  * @param  None
  * @retval None.
  */
static void WIFI_Handler(void)
{
  HAL_UART_DeInit(&WiFiUartHandle);

  while (1)
  {
    Error_Handler();
  }
}

#define RECEIVE_STRING "ESP RECEIVE"
#define SEND_STRING "ESP SEND"
void ESP8266Task(void const *argument)
{
  static uint8_t Buffer[1024 * 4];
  int32_t size;

  for (;;)
  {
    //BSP_LED_Toggle(LED1);
    //osDelay(1000);
    do
    {
      memset(Buffer, 0, 1024 * 4);
      //size = ESP8266_IO_Read(Buffer);

      printf("READ DATA FROM ESP8266: Lenght %d\nDATA: %s\n", size, Buffer);
    } while (strcmp((void *)Buffer, SEND_STRING) != 0);

    osDelay(1000);
    ESP8266_IO_Send((void *)RECEIVE_STRING, strlen(RECEIVE_STRING));
    printf("SEND DATA TO ESP8266: Lenght %d\nDATA: %s\n", size, RECEIVE_STRING);
  }
  /* USER CODE END StartDefaultTask */
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
