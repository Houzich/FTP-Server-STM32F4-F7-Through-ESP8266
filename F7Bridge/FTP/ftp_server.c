#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
/* ------------------------ FreeRTOS includes ----------------------------- */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ------------------------ lwIP includes --------------------------------- */
//#include "lwip/api.h"

/* ------------------------ Project includes ------------------------------ */
//#include "SD.h"         /* SD Card Driver (SPI mode) */
//#include "FAT.h"

/* ------------------------ USART includes ------------------------------ */
#include "esp8266_io.h"

/* ------------------------ Project includes ------------------------------ */
#include "Board.h"

#define FTP_USERNAME "123"
#define FTP_PASSWORD "123"

#ifdef DEBUG_FTP
#undef DEBUG       /* DEBUG */
#undef DEBUG_PRINT /* DEBUG_PRINT */
#define DEBUG(...)                          \
  do                                        \
  {                                         \
    debug_header("STM FTP: ", __VA_ARGS__); \
  } while (0)
#define DEBUG_PRINT(x) debug_print x;
#else
#define DEBUG(...)
#define DEBUG_PRINT(...)
#endif // DEBUG_FTP

/*Handle SD card*/
//static FATHandler *SD_FTP_Handle;

//Указатели на принятые пакеты
pESPPacket_Typedef *pReceivePackets[20];
int num_packet = 0;
QueueHandle_t ftp_queue;
extern QueueHandle_t uart_queue;
extern RingBuffer_t WiFiRxBuffer;
	
uint8_t buff_to_usart[FTP_BUFFERS_SIZE];
uint8_t buff_from_usart[FTP_BUFFERS_SIZE];

#define FTP_MUTEX_ENTER xSemaphoreTake((xSemaphoreHandle)SD_FTP_Handle->FAT_Mutex, portMAX_DELAY);
#define FTP_MUTEX_EXIT xSemaphoreGive((xSemaphoreHandle)SD_FTP_Handle->FAT_Mutex);

#define packet_receive ((ESPPacket_Typedef *)buff_from_usart)
#define packet_send ((ESPPacket_Typedef *)buff_to_usart)

FTPHandle_Typedef ftp;
static char *commands[] = {
    FTP_USER_REQUEST,
    FTP_PASS_REQUEST,
    FTP_QUIT_REQUEST,
    FTP_PORT_REQUEST,
    FTP_NLST_REQUEST,
    FTP_STOR_REQUEST,
    FTP_RETR_REQUEST,
    FTP_DELE_REQUEST,
    FTP_SYST_REQUEST,
    FTP_OPTS_REQUEST,
    FTP_PWD_REQUEST,
    FTP_SITE_REQUEST,
    FTP_TYPE_REQUEST,
    FTP_XPWD_REQUEST,
    FTP_CWD_REQUEST,
    FTP_MKD_REQUEST,
    FTP_XMKD_REQUEST,
    FTP_XCWD_REQUEST,
    FTP_ACCT_REQUEST,
    FTP_XRMD_REQUEST,
    FTP_RMD_REQUEST,
    FTP_STRU_REQUEST,
    FTP_MODE_REQUEST,
    FTP_XMD5_REQUEST,
    FTP_LIST_REQUEST,
    FTP_NOOP_REQUEST,
    ESP_OPEN_REQUEST,
    FTP_SIZE_REQUEST,
    FTP_NULL_REQUEST,
};

///////SYST
/**
 * FTP Server Main Control Socket Parser: requests and responses
 *  Available requests: USER, PASS, PORT, QUIT
 *
 * @param connection descriptor
 * @param buffer to hold FTP requests
 * @return none
 */
static uint8_t vFTPConnection(uint8_t *buf);

/**
 * FTP Server Main Data Socket Parser: requests and responses
 *  Available requests: NLST, RTR, STOR
 *
 * @param connection descriptor
 * @param string containing data socket port number and address
 * @return none
 */
static uint8_t FTP_DataFlowControl(uint8_t *buf);
static bool FTP_CloseDataPort(void);
static bool FTP_OpenDataPort(T32_8 ip, T16_8 port);
ftp_session_req FTP_CheckSessionState(uint8_t *buff);
/********************Private Functions ***************************************/

int32_t write_cmd(char *cmd)
{
  strcpy((void *)packet_send->data, cmd);
  uint32_t length = strlen(cmd);
  packet_send->header.id = ESP_FTP_COMMAND_ID;
  packet_send->header.len_data = length;
  return ESP8266_IO_Send((void *)packet_send, length + sizeof(ESPHeadr_Typedef));
}
int32_t write_cmd_block(ESPPacket_Typedef *pct, uint32_t length)
{
  pct->header.id = ESP_FTP_COMMAND_ID;
  pct->header.len_data = length;
  return ESP8266_IO_Send((void *)pct, length + sizeof(ESPHeadr_Typedef));
}
_ARMABI int write_cmd_sprintf(const char *format, ...)
{
  int length;
  va_list arg;
  va_start(arg, format);
  length = vsprintf((void *)packet_send->data, format, arg);
  va_end(arg);
  packet_send->header.id = ESP_FTP_COMMAND_ID;
  packet_send->header.len_data = length;
  return ESP8266_IO_Send((void *)packet_send, length + sizeof(ESPHeadr_Typedef));
}

int32_t write_data(ESPPacket_Typedef *pct, uint32_t length)
{
  pct->header.id = ESP_FTP_DATA_ID;
  pct->header.len_data = length;
  return ESP8266_IO_Send((void *)pct, length + sizeof(ESPHeadr_Typedef));
}

int32_t read(pESPPacket_Typedef **pRcvePacket)
{
  int32_t ret = 0;

  if (num_packet != 0)
  {
    num_packet -= 1;
    *pRcvePacket = pReceivePackets[num_packet];
    return ret;
  }

  do
  {
		memset(buff_from_usart, 0, sizeof(buff_from_usart));
    ret = ESP8266_IO_Read((void *)packet_receive, &num_packet, sizeof(buff_from_usart));
		HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
  } while (num_packet == 0);

  num_packet -= 1;
  *pRcvePacket = pReceivePackets[num_packet];
  return ret;
}

int32_t read_cmd(pESPPacket_Typedef **pRcvePacket)
{
  int32_t ret = 0;
  do
  {
    read(pRcvePacket);
  } while ((*pRcvePacket)->header.id != ESP_FTP_COMMAND_ID);
  return ret;
}

/* Translate first word to lower case */
static void FirstWordToLowerCase(char *buf, uint32_t max_len)
{
  char *cp;
  /* Translate first word to lower case */
  for (cp = buf; *cp != ' ' && *cp != '\0' && ((cp - buf) < max_len); cp++)
    *cp = tolower(*cp);
}

bool Parse_Request(char *buffer, char *cmd, char *arg, uint32_t cmd_max_len, uint32_t arg_max_len)
{
  char *cp;
  char *cmdtmp = cmd;
  char *argtmp = arg;
  FirstWordToLowerCase(buffer, cmd_max_len);
  for (cp = buffer; *cp != ' ' && *cp != '\0' && ((cp - buffer) < (cmd_max_len - 1)); cp++)
    *cmdtmp++ = *cp;
  *cmdtmp = '\0';
  cp++;

  for (; /* *cp != ' ' && */ *cp != '\0' && ((cp - buffer) < (arg_max_len - 1)); cp++)
    *argtmp++ = *cp;
  *argtmp = '\0';
  int len = strlen(arg);
  //убираем символы переноса коретки в конце
  if ((len > 2) && (len <= arg_max_len))
  {
    if ((arg[len - 1] == '\r') || (arg[len - 1] == '\n'))
      arg[len - 1] = '\0';
    if ((arg[len - 2] == '\r') || (arg[len - 2] == '\n'))
      arg[len - 2] = '\0';
  }

  char **cmdp;
  /* Find command in table; if not present, return syntax error */
  for (cmdp = commands; *cmdp != NULL; cmdp++)
    if (strncmp(*cmdp, cmd, strlen(*cmdp)) == 0)
    {
      ftp.current_cmd = (ftp_cmd)(cmdp - commands);
      return true;
    }
  ftp.current_cmd = FTP_NULL_CMD;
  return false;
}


ftp_session_req FTP_CheckSessionState(uint8_t *buff)
{
	if (strcmp((void *)buff, ESP_ERROR_REQUEST) == 0)
  {
    DEBUG("Session closed by client\n");
		ftp.state = FTPS_NOT_LOGIN;
    /*session closed by client*/		
    return FTPS_SESSION_FTP_CLOSE;
  }
	if (strcmp((void *)buff, ESP_CLOSE_DATAPORT_CMD) == 0)
  {
    DEBUG("Session dataport closed by client\n");
		if(ftp.state == FTPS_DATAFLOW)		
			ftp.state = FTPS_LOGIN;
    /*session closed by client*/
    return FTPS_SESSION_DATAPORT_CLOSE;
  }
	if (strcmp((void *)buff, ESP_OPEN_DATAPORT_OK_RESPONSE) == 0)
	{
		ftp.state = FTPS_DATAFLOW;
		return FTPS_SESSION_DATAPORT_OPEN;
	}
	return FTPS_SESSION_NONE;
}



/**
 * FTP Server Main Control Socket Parser: requests and responses
 *  Available requests: USER, PASS, PORT, QUIT
 *
 * @param connection descriptor
 * @param buffer to hold FTP requests
 * @return none
 */
static uint8_t
FTPConnection(uint8_t *buff)
{
  FRESULT fs_res;
  FRESULT fs_res2;

  //  uart_event_t ftp_event;
  int len = 0;
  /*send FTP server first RESPONSE*/
  //****RESPONSE OK CONNECTED
  write_cmd(FTP_WELCOME_RESPONSE);
  //DEBUG("RESPONSE OK CONNECTED\n");
  pESPPacket_Typedef *pPacket;
  do
  {
    /*if reception is OK: wait for REQUEST from client*/
    len = read(&pPacket);

    if (pPacket->header.id != ESP_FTP_COMMAND_ID)
      continue;

    Parse_Request((void *)pPacket->data, ftp.cmd, ftp.arg, member_size(FTPHandle_Typedef, cmd), member_size(FTPHandle_Typedef, arg));
		ftp_session_req  session_req = FTP_CheckSessionState(pPacket->data);
    if (session_req == FTPS_SESSION_FTP_CLOSE)
      break;
    if (session_req != FTPS_SESSION_NONE)
      continue;
		
		
    /*system required*/
    if (ftp.current_cmd == ESP_OPEN_CMD)
    {
      DEBUG("OPEN FTP Server\n");
      ftp.state = FTPS_NOT_LOGIN;
      write_cmd(FTP_WELCOME_RESPONSE);
			continue;
    }
		
		if (ftp.current_cmd == FTP_NOOP_CMD)
		{
        DEBUG("NOOP request\n");
        write_cmd(FTP_NOOP_OK_RESPONSE);
				continue;
    }
    /*authentication required*/
    if ((ftp.state == FTPS_LOGIN)||(ftp.state == FTPS_DATAFLOW)) //already logged in
    {
      switch (ftp.current_cmd)
      {
      case FTP_PORT_CMD:
        DEBUG("PORT request\n");
        FTP_DataFlowControl((void *)&pPacket->data[sizeof(FTP_PORT_REQUEST)]);
				continue;
        break;

      case FTP_DELE_CMD:
        DEBUG("DELETE request\n");
				if (FR_OK == fsDeleteFile(ftp.arg))
				{
					 //****RESPONSE: OK
					 write_cmd(FTP_DELE_OK_RESPONSE);
				}
				else
				{
					 //****RESPONSE: FAILED
					 write_cmd(FTP_WRITE_FAIL_RESPONSE);
				}
        break;
				case FTP_RMD_CMD:
					
				break;
      case FTP_SYST_CMD:
        DEBUG("SYST request\n");
        write_cmd(FTP_SYST_OK_RESPONSE);
        break;

      case FTP_OPTS_CMD:
        DEBUG("OPTS request\n");
        write_cmd(FTP_OPTS_RESPONSE);
        break;
      case FTP_PWD_CMD:
      case FTP_XPWD_CMD:
        DEBUG("PWD request\n");
        fs_res = SD_getcwd((void *)ftp.workingdir);
        if (fs_res == FR_OK)
        {
          write_cmd_sprintf(FTP_PWD_RESPONSE, &ftp.workingdir[2]);
        }
        else
        {
          write_cmd(FTP_QUIT_RESPONSE);
        }
        break;
      case FTP_SITE_CMD:
        DEBUG("SITE request\n");
        if (strstr(ftp.arg, "help") != 0)
        {
//          int pnt = 0;
//          len = sprintf((void *)&packet_send->data[pnt], "214-The following SITE commands are recognized (* =>'s unimplemented)\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "UTIME <sp> YYYYMMDDhhmm[ss] <sp> path\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "SYMLINK <sp> source <sp> destination\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "RMDIR <sp> path\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "The following SITE extensions are recognized:\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "RATIO -- show all ratios in effect\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "HELP\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "CHGRP\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "CHMOD\r\n");
//          pnt += len;
//          len = sprintf((void *)&packet_send->data[pnt], "214 Direct comments to %d.%d.%d.%d\r\n", ftp.ip[0], ftp.ip[1], ftp.ip[2], ftp.ip[3]);
//          pnt += len;
//          write_cmd_block(packet_send, pnt);
          write_cmd_sprintf("500 Unknown command argument \"%s\"\r\n", ftp.arg);
        }
        else
        {
          write_cmd_sprintf("500 Unknown command argument \"%s\"\r\n", ftp.arg);
        }
        break;
      case FTP_TYPE_CMD:
        switch (ftp.arg[0])
        {
        case 'A':
        case 'a': /* Ascii */
          ftp.type = ASCII_TYPE;
          //						arg[0] = 'A';
          //						arg[1] = '\0';
          write_cmd_sprintf(FTP_TYPE_RESPONSE, ftp.arg);
          break;

        case 'B':
        case 'b': /* Binary */
        case 'I':
        case 'i': /* Image */
          ftp.type = IMAGE_TYPE;
          write_cmd_sprintf(FTP_TYPE_RESPONSE, ftp.arg);
          break;

        default: /* Invalid */
          write_cmd_sprintf(FTP_TYPE_FAIL_RESPONSE, ftp.arg);
          break;
        }
        break;
      case FTP_CWD_CMD:

        fs_res = f_chdir(ftp.arg);
        fs_res2 = SD_getcwd((void *)ftp.workingdir);
        if ((fs_res == FR_OK) && (fs_res2 == FR_OK))
        {
          write_cmd_sprintf(FTP_CWD_OK_RESPONSE, &ftp.workingdir[2]);
        }
        else
        {
          write_cmd_sprintf(FTP_CWD_FAIL_RESPONSE, ftp.arg);
        }
        break;

      case FTP_MKD_CMD:
      case FTP_XMKD_CMD:
        write_cmd((void *)FTP_MKD_FAIL_RESPONSE);
        break;

      case FTP_XCWD_CMD:
      case FTP_ACCT_CMD:
      case FTP_XRMD_CMD:
      case FTP_STRU_CMD:
      case FTP_MODE_CMD:
      case FTP_XMD5_CMD:

      write_cmd(FTP_CMD_NOT_IMP_RESPONSE);
      break;
//****************************************************************************
//****************************************************************************
//***************DATAFLOW
//****************************************************************************
//****************************************************************************
//****************************************************************************
      /*LIST*/
      case FTP_LIST_CMD:
        if (FTP_OpenDataPort(ftp.client_ip, ftp.client_port))
        {
          SD_getcwd((void *)ftp.workingdir);
          //****Response Sending
          write_cmd_sprintf(FTP_LIST_OK_RESPONSE, (char *)&ftp.workingdir[2]);
          //Send the file list
          FTP_Send_List_Of_Files(ftp.workingdir);
          //Start Closing Data Stream
          FTP_CloseDataPort();
          //****RESPONSE: OK
          write_cmd_sprintf(FTP_LIST_TRANSFER_OK_RESPONSE, (char *)&ftp.workingdir[2]);
					
        }
        else
        {
          //****RESPONSE PORT cant be opened!!
          write_cmd(FTP_DATA_PORT_FAILED);
        }
        break;
      /*NLST*/
      case FTP_NLST_CMD:
        if (FTP_OpenDataPort(ftp.client_ip, ftp.client_port))
        {
          //****Response Sending
          write_cmd(FTP_NLST_OK_RESPONSE);

          //Send the file list
          SD_getcwd((void *)ftp.workingdir);
          FTP_Send_NLST_List_Of_Files(ftp.workingdir);

          //Start Closing Data Stream
          FTP_CloseDataPort();

          //****RESPONSE: OK
          write_cmd(FTP_TRANSFER_OK_RESPONSE);
        }
        else
        {
          //****RESPONSE PORT cant be opened!!
          write_cmd(FTP_DATA_PORT_FAILED);
        }
        break;
        /*READ*/
      case FTP_RETR_CMD:
        if (!FTP_Read_Is_Possible(ftp.arg))
        {
          if (FTP_OpenDataPort(ftp.client_ip, ftp.client_port))
          {
            //****Response Sending
            write_cmd(FTP_RETR_OK_RESPONSE);

            //send info
            FTP_Read_File();

            //Start Closing Data Stream
            FTP_CloseDataPort();

            //****RESPONSE: OK
            write_cmd(FTP_TRANSFER_OK_RESPONSE);
          }
          else
          {
            //****RESPONSE: DUPLICATION FILE NAME
						write_cmd(FTP_WRITE_FAIL_RESPONSE);
          }
        }
        else
        {
          //****RESPONSE: DUPLICATION FILE NAME
          write_cmd(FTP_WRITE_FAIL_RESPONSE);
        }
        break;
        /*WRITE*/
      case FTP_STOR_CMD:
				if (FTP_OpenDataPort(ftp.client_ip, ftp.client_port))
				{
					if (!FTP_Does_File_Exist(ftp.arg))
					{
						fsDeleteFile(ftp.arg);
						//****RESPONSE: FILE ALREADY EXISTS
						//write_cmd(FTP_WRITE_FAIL_RESPONSE);
					}
					//****Response Sending
					write_cmd(FTP_STOR_OK_RESPONSE);

					/*filename to write*/
					int err = FTP_Write_File((void *)ftp.arg);

					//Start Closing Data Stream: client closes tcp session: against protocol!!
					FTP_CloseDataPort();
					if(err != 0){
						write_cmd(FTP_WRITE_FAIL_RESPONSE);
					}else{
						//****RESPONSE: OK
						write_cmd(FTP_TRANSFER_OK_RESPONSE);
					}
				}
				else
				{
					//****RESPONSE: DUPLICATION FILE NAME
					write_cmd(FTP_WRITE_FAIL_RESPONSE);
				}

        break;
      case FTP_SIZE_CMD:
        DEBUG("SIZE request\n");
        uint32_t size = 0;
        if ((strlen(ftp.arg) > 3) && (fsGetFileSize(ftp.arg, &size) == FR_OK))
        {
          write_cmd_sprintf(FTP_SIZE_OK_RESPONSE, size);
        }
        else
        {
          write_cmd(FTP_SIZE_FAIL_RESPONSE);
        }
        break;	
			
			
			
      default:
				//if(ftp.state == FTPS_DATAFLOW) break;
        DEBUG("Wrong request LOGGIN/DATAFLOW!!!\n");
        DEBUG("Request: %s\n", (void *)pPacket->data);
        if (!FTP_QUIT_OR_WRONG_REQUEST((void *)pPacket->data))
        {
          //break; /*QUIT command*/
        }
        break;
      }
    }
    else if (ftp.state == FTPS_NOT_LOGIN) //not logged in
    {
      switch (ftp.current_cmd)
      {
      case FTP_USER_CMD:
        DEBUG("authentication process\n");
        //*********************************************
        write_cmd(FTP_PASS_OK_RESPONSE);
        ftp.state = FTPS_LOGIN;
        //*********************************************
        /*authentication process: username matchs exactly?*/
        //        if (!strncmp((void *)&(void *)pPacket->data[sizeof(FTP_USER_REQUEST)], FTP_USERNAME, strlen(FTP_USERNAME)))
        //        {
        //          //*****RESPONSE USER OK
        //          write((void *)FTP_USER_RESPONSE, strlen(FTP_USER_RESPONSE));
        //          DEBUG("RESPONSE USER OK\n");

        //          /*if reception is OK: wait for REQUEST from client*/
        //          len = read_cmd();
        //				FirstWordToLowerCase((char *)(void *)pPacket->data);
        //          if (FTPCheckSessionClosed((void *)pPacket->data)) break;
        //          if (strstr((void *)(void *)pPacket->data, FTP_PASS_REQUEST) != NULL)
        //          {
        //            /*authentication process: password matchs exactly?*/
        //            if (!strncmp((void *)&(void *)pPacket->data[sizeof(FTP_PASS_REQUEST)], FTP_PASSWORD, strlen(FTP_PASSWORD)))
        //            {
        //              //***RESPONSE: PASSWORD OK
        //              write((void *)FTP_PASS_OK_RESPONSE, strlen(FTP_PASS_OK_RESPONSE));
        //              DEBUG("PASSWORD OK\n");
        //              correct_login = true;
        //            }
        //            else
        //            {
        //              //***RESPONSE: PASSWORD FAILED
        //              write((void *)FTP_PASS_FAIL_RESPONSE, strlen(FTP_PASS_FAIL_RESPONSE));
        //              DEBUG("PASSWORD FAILED\n");
        //            }
        //          }
        //          else
        //          {
        //            //***RESPONSE: EXPECTING PASS request
        //            write((void *)FTP_BAD_SEQUENCE_RESPONSE, strlen(FTP_BAD_SEQUENCE_RESPONSE));
        //            DEBUG("RESPONSE EXPECTING PASS\n");
        //          }
        //        }
        //        else
        //        {
        //          //***RESPONSE USER FAILED
        //          write((void *)FTP_PASS_FAIL_RESPONSE, strlen(FTP_PASS_FAIL_RESPONSE));
        //          DEBUG("RESPONSE USER FAILED\n");
        //        }
        break;
      default:
        DEBUG("Wrong request NOT LOGGIN!!!\n");
        DEBUG("Request: %s\n", (void *)pPacket->data);
        if (!FTP_QUIT_OR_WRONG_REQUEST((void *)pPacket->data))
        {
          //break; /*QUIT command*/
        }
        break;
      }
    }
//    if (ftp.state == FTPS_DATAFLOW)
//    {
//      switch (ftp.current_cmd)
//      {
//      default:
//				DEBUG("Wrong request DATAFLOW!!!\n");
//        DEBUG("Request: %s\n", (void *)pPacket->data);
//        //****Response: unknown request
//        write_cmd((void *)FTP_CMD_NOT_IMP_RESPONSE);
//        break;
//      }
//    }

  } while (1);

  //FTP_TCP_EXIT_LOW:
  /*client closing the session*/

  /*close the session!!*/

  return 1; /*default close value*/
}

/**
 * Closes or Leave session depending on client request
 *
 * @param connection descriptor
 * @param buffer space 
 * @return 0 keep session, otherwise session needs to be closed
 */
uint8_t
FTP_QUIT_OR_WRONG_REQUEST(uint8_t *buf)
{
  if (strstr((void *)buf, FTP_QUIT_REQUEST) != NULL)
  {
    //****RESPONSE CLOSING SESSION: BYE
    write_cmd(FTP_QUIT_RESPONSE);
    return 1; /*close session*/
  }
  else
  {
    //****UNKNOWN REQUEST
    write_cmd(FTP_UNKNOWN_RESPONSE);
    return 0; /*keep session*/
  }
}

/**
 * Open data socket: ftp server connects to client.
 *
 * @param ip address to connect to
 * @param tcp port to connect to
 * @return connection descriptor, if NULL error, other OK.
 */

static bool FTP_OpenDataPort(T32_8 ip, T16_8 port)
{
  /*START:open specific port requested by PORT*/
  /* Create a new TCP connection handle. */
  //  struct netconn *conn_data;

  //  /*create data port*/
  //  if( (conn_data = netconn_new(NETCONN_TCP)) == NULL )
  //  {
  //     return NULL;/*error*/
  //  }
  //  /*wait until it's linked to server*/
  //  if( netconn_connect(conn_data,add,port) != ERR_OK )
  //  {
  //     return NULL;/*error*/
  //  }
  /*set timeout for this connection*/
  //netconn_set_timeout((void *)conn_data,4000/*timeout*/);

  //open data port
  write_cmd_sprintf(ESP_OPEN_DATAPORT_CMD,
                    ip.u8[0], ip.u8[1], ip.u8[2], ip.u8[3],
                    port.u8[0], port.u8[1]);
  pESPPacket_Typedef *pPacket;
  int len = read_cmd(&pPacket);

	if(FTP_CheckSessionState(pPacket->data) == FTPS_SESSION_DATAPORT_OPEN)
		return true;
	
  return false;
}


/**
 * Close data socket
 *
 * @param connection descriptor
 * @return 0 if connection was closed
 */
static bool
FTP_CloseDataPort(void)
{
  /*delete TCP connection*/
  //netconn_close(conn_data);
  //netconn_delete(conn_data);

  write_cmd(ESP_CLOSE_DATAPORT_CMD);
  pESPPacket_Typedef *pPacket;
  int len = read_cmd(&pPacket);
	if(FTP_CheckSessionState(pPacket->data) == FTPS_SESSION_DATAPORT_CLOSE)
		return true;
  return false;
}

/**
 * FTP Server Main Data Socket Parser: requests and responses
 *  Available requests: NLST, RTR, STOR
 *
 * @param connection descriptor
 * @param string containing data socket port number and address
 * @return none
 */

static uint8_t
FTP_DataFlowControl(uint8_t *buf)
{
  /*temporal pointers*/
  char *end;
  /*temporal counter*/
  char i;

  /*****START: get PORT information: parsing*/
  //Get IP: four parts
  for (i = 0; i < sizeof(ftp.client_ip); i++)
  {
    end = (char *)strchr((const char *)buf, ',');
    ftp.client_ip.u8[i] = (uint8_t)strtoul((void *)buf, (char **)&end, 10);
    buf = (uint8_t *)end + 1;
  }
  //Get FTP Port:first part
  end = (char *)strchr((const char *)buf, ',');
  ftp.client_port.u8[0] = (uint8_t)strtoul((void *)buf, (char **)&end, 10);
  buf = (uint8_t *)end + 1;
  //Get FTP Port:second part
  end = (char *)strchr((const char *)buf, '\r');
  ftp.client_port.u8[1] = (uint8_t)strtoul((void *)buf, (char **)&end, 10);
  /*****END**********************************/

  //*****RESPONSE: OPEN PORT
  write_cmd(FTP_PORT_OK_RESPONSE);
  ftp.state = FTPS_DATAFLOW;

//  pESPPacket_Typedef *pPacket;
//  /*if reception is OK: wait for REQUEST from client from CONTROL port*/
//  read_cmd(&pPacket);
//  Parse_Request((void *)pPacket->data, ftp.cmd, ftp.arg, member_size(FTPHandle_Typedef, cmd), member_size(FTPHandle_Typedef, arg));
//  if (FTPCheckSessionClosed((void *)pPacket->data))
//  {
//    /*session closed by client*/
//    return 1; /*FAIL*/
//  }
  return 0; /*OK*/
}

/**
 * Look if a file already exist with the name
 *  No case sensitive
 *
 * @param file name to look for on FAT
 * @return 0 if file was found, otherwise not
 */
uint8_t
FTP_Does_File_Exist(char *fname)
{
  uint8_t result = 1;
  FRESULT res;
  FILINFO filinfo;
  res = f_stat(fname, &filinfo);
  if (res == FR_OK)
    return 0;
  return result; /*File not found*/
}

/**
 * Callback to send files name to ethernet
 *
 * @param descriptor to use for sending
 * @param filename string
 * @return always 1
 */
uint8_t
FTP_SD_send(void *var, uint8_t *fileName)
{
  //    struct netconn *connfd = (struct netconn *)var;
  //
  //    /*append "\r\n"*/
  //    strcat( fileName, STRING_END );
  //    /*send files name*/
  //    write(connfd,fileName,strlen(fileName),NETCONN_COPY);
  return 1;
}

/**
 * Callback to check if file exists
 *
 * @param data to compare
 * @param filename string
 * @return 0 if file found, otherwise zero
 */
uint8_t
FTP_CompareFile(void *var, uint8_t *fileName)
{
  //  uint8_t *str = (uint8_t *)var;

  //  if (!strcmp((void *)str, (void *)SDbuffer))
  //    return 0; /*File Found*/
  //  else
  return 1; /*File not found*/
}

/**
 * Returns thru connection descriptor all files names in FAT
 *
 * @param connection descriptor to send files' names
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Send_NLST_List_Of_Files(char *path)
{
  //TODO: what happens if no files at all?

  //    /*FSL: SD Mutex Enter*/
  //    FTP_MUTEX_ENTER

  //    /*dont care what it returns*/
  //    (void)FAT_LS(SD_FTP_Handle,SDbuffer,connfd,FTP_SD_send);

  //    /*SD Mutex Exit*/
  //    FTP_MUTEX_EXIT

  FRESULT res;
  DIR dir;
  static FILINFO fno;

  res = f_opendir(&dir, path); /* Open the directory */
  if (res == FR_OK)
  {
    for (;;)
    {
      res = f_readdir(&dir, &fno); /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0)
        break; /* Break on error or end of dir */
      //DEBUG("%s", fno.fname);
      int len = sprintf((void *)packet_send->data, "%s", fno.fname);
      write_data(packet_send, len);
    }
    f_closedir(&dir);
  }
  else
  {
    return 1;
  }

  return 0; /*OK*/
}

uint8_t
FTP_Send_List_Of_Files(char *path)
{
  FRESULT res;
  DIR dir;
  static FILINFO fno;
  int len;
  res = f_opendir(&dir, path); /* Open the directory */
  if (res == FR_OK)
  {
    for (;;)
    {
      res = f_readdir(&dir, &fno); /* Read a directory item */
      if (res != FR_OK || fno.fname[0] == 0)
        break; /* Break on error or end of dir */

      if (fno.fattrib & AM_DIR)
      { /* It is a directory */
        len = sprintf((char *)packet_send->data, "drwxr-xr-x 1 ftp ftp 0 Dec 31 2014 %s\r\n", fno.fname);
      }
      else
      { /* It is a file. */
        uint32_t size = 0;
        if (fsGetFileSize(fno.fname, &size) != FR_OK)
          break;
        len = sprintf((char *)packet_send->data, "-rwxr-xr-x 1 ftp ftp %d Dec 31 2014 %s\r\n", size, fno.fname);
      }
      write_data(packet_send, len);
    }
    f_closedir(&dir);
  }
  else
  {
    return 1;
  }

  return 0; /*OK*/
}

/**
 * Check if filename exists in FAT system for reading
 *
 * @param file name to check
 * @return 0 if read is possible, otherwise not
 */
uint8_t
FTP_Read_Is_Possible(char *fname)
{
  uint8_t result = 1;

  if (f_open(&USBHFile, fname, FA_READ) == FR_OK)
  {
    return 0;
  }

  return result; /*Fail*/
}

/**
 * Gets a file from FAT and send it to a connection descriptor
 *  Call FTP_Read_Is_Possible(...)
 *
 * @param connection descriptor to write data read data
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Read_File(void)
{
  FRESULT res; /* FatFs function common result code */
  uint32_t bytesread;
  do
  {
    /* Read data from the text file */
    res = f_read(&USBHFile, packet_send->data, member_size(ESPPacket_Typedef, data), (void *)&bytesread);
    //res = f_read(&USBHFile, packet_send->data, 119, (void *)&bytesread);
    DEBUG("Read data from the %s\n", ftp.arg);
    if ((bytesread == 0) || (res != FR_OK))
    {
      break;
    }
		//HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
		//osDelay(100);
		if (WiFiRxBuffer.head != WiFiRxBuffer.tail){
			int len = 0;
			pESPPacket_Typedef *pPacket;
			len = read(&pPacket);
		}
		//HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
    write_data(packet_send, bytesread);
		//HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
		//osDelay(100);
		if (WiFiRxBuffer.head != WiFiRxBuffer.tail){
			int len = 0;
			pESPPacket_Typedef *pPacket;
			len = read(&pPacket);
		}
		//HAL_GPIO_WritePin(COM_Output_GPIO_Port, COM_Output_Pin, GPIO_PIN_SET);
  } while (1);
  f_close(&USBHFile);
  return 0; /*OK*/
}

/**
 * Puts a file from a connection descriptor and send it to FAT
 *  First check if file do not exist to avoid duplication of files in FAT
 *
 * @param connection descriptor to use read data
 * @param file name to write.
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Write_File(uint8_t *path)
{
  int len = 0;
  uint32_t lenfile = 0;
  //    /*receiver buffer*/
  //    struct netbuf *inbuf;
  //    struct pbuf *q;
  //
  //    /*Prior:string already doesnt exist on FAT*/
  //    /*FSL: SD Mutex Enter*/
  //    FTP_MUTEX_ENTER
  //
  //    /*Write File: remaining bits: doesn't check for file names' duplication*/
  //    if( FAT_FileOpen(SD_FTP_Handle,SDbuffer,(uint8_t *)strupr(data),CREATE) == FILE_CREATE_OK )
  //    {
  //       do
  //       {
  //         /*stay here until session is: closed, received or out of memory*/
  //         length = netconn_rcv_req((void *)connfd, NULL, (void **)&inbuf, NETCONN_RCV_NETBUFFER);
  //	       if (connfd->last_err == ERR_OK)
  //         {
  //           /*start segment index: copy all the network buffer segments*/
  //           q = inbuf->ptr;
  //           do
  //           {
  //             /*write as many bytes as needed*/
  //             FAT_FileWrite(SD_FTP_Handle,SDbuffer,q->payload,q->len);
  //           }
  //           while( ( q = q->next ) != NULL );
  //
  //           /*free pbuf memory*/
  //           netbuf_delete(inbuf);
  //         }
  //         else
  //         {
  //            /*session closed, out of memory, timeout: leave loop*/
  //            break;
  //         }
  //       }while(1);
  //
  //       /*close file*/
  //       FAT_FileClose(SD_FTP_Handle,SDbuffer);
  //    }
  //
  //    /*SD Mutex Exit*/
  //    FTP_MUTEX_EXIT
  FRESULT res; /* FatFs function common result code */
  uint32_t numOfWrittenBytes;
  pESPPacket_Typedef *pPacket;
  if (f_open(&USBHFile, (void *)path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
  {
    /* file Open for write Error */
    return 1;
  }
  else
  {
    do
    {
      len = read(&pPacket);
      //Receive command
      if (pPacket->header.id == ESP_FTP_COMMAND_ID)
      {
        if (FTP_CheckSessionState(pPacket->data) != FTPS_SESSION_NONE)
        {
          DEBUG("File Write osession closed by client\n");
          break;
        }
        else
        {
          DEBUG("Receive unrecognaized command while writting file. Exit\n Command: %s", pPacket->data);
          goto EXIT_ERROR;
        }
      }
			if (pPacket->header.id != ESP_FTP_DATA_ID)
      {
				continue;
			}
      /* Write data to the text file */
			//res = fsWriteFile(&USBHFile, void *data, size_t length);
			//DEBUG("START Write data to the %s len:%d\n", path, pPacket->header.len_data);
      res = fsWriteFile(&USBHFile, (void *)&pPacket->data, pPacket->header.len_data, (void *)&numOfWrittenBytes);
      DEBUG("Write data to the %s len:%d\n", path, numOfWrittenBytes);
      lenfile += numOfWrittenBytes;
      if (res != FR_OK)
      {
        /* file Write or EOF Error */
        DEBUG("File Write or EOF Error %s\n", path);
        goto EXIT_ERROR;
      }
    } while (1);
    /* Close the open text file */
    DEBUG("Close the file %s, written %d bytes\n", path, numOfWrittenBytes);
    f_close(&USBHFile);
  }

  //if(f_write(&wav_file, (uint8_t*)(haudio.buff),
  //                     AUDIO_IN_BUFFER_SIZE/2,
  //                     (void*)&numOfWrittenBytes) == FR_OK)
  //          {
  //            if(numOfWrittenBytes == 0)
  //            {
  //              AUDIO_RECORDER_StopRec();
  //            }
  //          }
  return 0; /*Error*/
EXIT_ERROR:
  f_close(&USBHFile);
  return 1;
}

/**
 * Start an embedded FTP server Task: 1 client and 1 file per transfer
 *
 * @param paremeter pointer to use with task
 * @return none
 */
void FTPServerTask(void const *argument)
{
  int len = 0;
  pESPPacket_Typedef *pPacket;
  for (;;)
  {
		HAL_GPIO_WritePin(ESP_Reset_GPIO_Port, ESP_Reset_Pin, GPIO_PIN_RESET);
		osDelay(10);
		HAL_GPIO_WritePin(ESP_Reset_GPIO_Port, ESP_Reset_Pin, GPIO_PIN_SET);
		osDelay(1000);
		while(!sd.sd_ready)osDelay(100);
    write_cmd_sprintf(ESP_CONNECT_CMD, filetext);
		
    do
    {
      memset(buff_from_usart, 0, sizeof(buff_from_usart));
      read_cmd(&pPacket);
      Parse_Request((void *)pPacket->data, ftp.cmd, ftp.arg, member_size(FTPHandle_Typedef, cmd), member_size(FTPHandle_Typedef, arg));

      /*system required*/
      if (ftp.current_cmd == ESP_OPEN_CMD)
      {
        break;
      }
      //DEBUG("Receive %d bytes\n%s\n", len, (void *)pPacket->data);
    } while (1);

    FTPConnection(buff_from_usart);
  }
  /*never get here!*/
  //return;
}
osThreadId FTPServerTaskHandle;
/*###############################################################*/
/*###############################################################* FTP_Init -->*/
/*###############################################################*/
void FTP_Init(void)
{
  osThreadDef(ftpServerTaskID, FTPServerTask, osPriorityNormal, 0, FTP_STK_SZ);
  FTPServerTaskHandle = osThreadCreate(osThread(ftpServerTaskID), NULL);

  ftp.current_cmd = FTP_NULL_CMD;
}
