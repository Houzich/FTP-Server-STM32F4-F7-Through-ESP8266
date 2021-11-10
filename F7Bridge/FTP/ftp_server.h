#ifndef _FTP_SERVER_H_
#define _FTP_SERVER_H_

#ifdef STM32F7
#include "stm32f7xx_hal.h"
#elif defined(STM32F4)
#include "stm32f4xx_hal.h" 
#endif   
#include "fatfs.h"


enum
{
  FTP_CLOSED,
  FTP_OPEN
};

#define FTP_TASK_PRIORITY          tskIDLE_PRIORITY + 3

#define FTP_TCP_CONTROL_PORT       21

#define FTP_REQUEST_SPACE          32/*Client request should not be longer*/

/*FTP Server Requests*/
#define FTP_USER_REQUEST           "user"
#define FTP_PASS_REQUEST           "pass"              
#define FTP_QUIT_REQUEST           "quit"
#define FTP_PORT_REQUEST           "port"
#define FTP_NLST_REQUEST           "nlst"
#define FTP_STOR_REQUEST           "stor"
#define FTP_RETR_REQUEST           "retr"
#define FTP_DELE_REQUEST           "dele"
#define FTP_SYST_REQUEST           "syst"
#define FTP_OPTS_REQUEST        	 "opts"
#define FTP_PWD_REQUEST        	 	 "pwd"
#define FTP_SITE_REQUEST        	 "site"
#define FTP_TYPE_REQUEST					 "type"
#define FTP_XPWD_REQUEST        	 "xpwd"
#define FTP_CWD_REQUEST        	 	 "cwd"
#define FTP_MKD_REQUEST        	 	 "mkd"
#define FTP_XMKD_REQUEST        	 "xmkd"
#define FTP_XCWD_REQUEST        	 "xcwd"
#define FTP_ACCT_REQUEST        	 "acct"
#define FTP_XRMD_REQUEST        	 "xrmd"
#define FTP_RMD_REQUEST        	 	 "rmd"
#define FTP_STRU_REQUEST        	 "stru"
#define FTP_MODE_REQUEST        	 "mode"
#define FTP_XMD5_REQUEST        	 "xmd5"
#define FTP_LIST_REQUEST        	 "list"
#define FTP_NOOP_REQUEST        	 "noop"
#define ESP_OPEN_REQUEST        	 "open"
#define FTP_SIZE_REQUEST        	 "size"

#define ESP_OPEN_DATAPORT_CMD								"OPEN DATAPORT %c%c%c%c%c%c"
#define ESP_CLOSE_DATAPORT_CMD							"CLOSE DATAPORT"
#define ESP_OPEN_DATAPORT_OK_RESPONSE				"OPEN DATAPORT OK"
#define ESP_CLOSE_DATAPORT_OK_RESPONSE			"CLOSE DATAPORT OK"
#define ESP_RESET_CMD												"RESET FROM STM32"
#define ESP_CONNECT_CMD											"CONNECT %s"


#define FTP_NULL_REQUEST        	 NULL
#define ESP_ERROR_REQUEST					 "session closed by client"


/*FTP Server Response*/

#define FTP_WELCOME_RESPONSE        "220 Service Ready\r\n"
//#define FTP_USER_RESPONSE           "331 USER OK. PASS needed\r\n"
#define FTP_USER_RESPONSE           "332 Please log in with USER and PASS\r\n"
#define FTP_PASS_FAIL_RESPONSE      "530 NOT LOGGUED IN\r\n"
#define FTP_PASS_OK_RESPONSE        "230 USR LOGGUED IN\r\n"
#define FTP_PORT_OK_RESPONSE        "200 PORT OK\r\n"
#define FTP_NLST_OK_RESPONSE        "150 NLST OK\r\n"
#define FTP_RETR_OK_RESPONSE        "150 RETR OK\r\n"
#define FTP_STOR_OK_RESPONSE        "150 STOR OK\r\n"
#define FTP_DELE_OK_RESPONSE        "250 DELE OK\r\n"
#define FTP_QUIT_RESPONSE           "221 BYE OK\r\n"
#define FTP_TRANSFER_OK_RESPONSE    "226 Transfer OK\r\n"
#define FTP_WRITE_FAIL_RESPONSE     "550 File unavailable\r\n"
#define FTP_CMD_NOT_IMP_RESPONSE    "502 Command Unimplemented\r\n"
#define FTP_DATA_PORT_FAILED        "425 Cannot open Data Port\r\n"
#define FTP_UNKNOWN_RESPONSE        "500 Unrecognized Command\r\n"
#define FTP_BAD_SEQUENCE_RESPONSE   "503 Bad Sequence of Commands\r\n"
#define FTP_SYST_OK_RESPONSE        "215 UNIX emulated by XXX.\r\n"
#define FTP_OPTS_RESPONSE        		"200 UTF8 set to on\r\n"
#define FTP_PWD_RESPONSE						"257 \"%s\" is current directory.\r\n"
#define FTP_TYPE_RESPONSE						"200 Type set to %s\r\n"
#define FTP_TYPE_FAIL_RESPONSE			"501 Unknown type \"%s\"\r\n"
#define FTP_CWD_OK_RESPONSE					"250 CWD successful. \"%s\" is current directory.\r\n"
#define FTP_CWD_FAIL_RESPONSE				"550 CWD fail. \"%s\" no directory.\r\n"
#define FTP_MKD_FAIL_RESPONSE				"550 Can't create directory. Permission denied\r\n"
#define FTP_XMKD_FAIL_RESPONSE			"550 Can't create directory. Permission denied\r\n"
#define FTP_DELE_FAIL_RESPONSE			"550 Could not delete. Permission denied\r\n"
#define FTP_LIST_OK_RESPONSE        		 "150 Opening data channel for directory listing of \"%s\"\r\n"
#define FTP_LIST_TRANSFER_OK_RESPONSE    "226 Successfully transferred \"%s\"\r\n"
#define FTP_NOOP_OK_RESPONSE        "200 NOOP OK\r\n"
#define FTP_SIZE_OK_RESPONSE        "213 %d\r\n"
#define FTP_SIZE_FAIL_RESPONSE     	"550 File not Found\r\n"

typedef union
{
   uint8_t u8[4];
   uint32_t u32;      
}T32_8;
typedef union
{
   uint8_t u8[2];
   uint16_t u16;      
}T16_8;

#define USART_BUFFER_SIZE  (2048 * 1) + sizeof(ESPHeadr_Typedef)
#define FTP_BUFFERS_SIZE USART_BUFFER_SIZE
#define	ESP_FTP_COMMAND_ID	0x0200
#define	ESP_FTP_DATA_ID	0x0300
#define	ESP_LOG_ID	0x0100

#pragma pack(push, 1)
typedef struct {
	uint16_t id;
	uint16_t len_data;
}ESPHeadr_Typedef;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	ESPHeadr_Typedef header;
	uint8_t data[FTP_BUFFERS_SIZE - sizeof(ESPHeadr_Typedef)];
}ESPPacket_Typedef;
#pragma pack(pop)
#pragma pack(push, 1)
typedef struct {
	ESPHeadr_Typedef header;
	uint8_t data[];
}pESPPacket_Typedef;
#pragma pack(pop)

/* FTP commands */
typedef enum  {
	FTP_USER_CMD,
	FTP_PASS_CMD,
	FTP_QUIT_CMD,
	FTP_PORT_CMD,
	FTP_NLST_CMD,
	FTP_STOR_CMD,
	FTP_RETR_CMD,
	FTP_DELE_CMD,
	FTP_SYST_CMD,
	FTP_OPTS_CMD,
	FTP_PWD_CMD,
	FTP_SITE_CMD,
	FTP_TYPE_CMD,
	FTP_XPWD_CMD,
	FTP_CWD_CMD,
	FTP_MKD_CMD,
	FTP_XMKD_CMD,
	FTP_XCWD_CMD,
	FTP_ACCT_CMD,
	FTP_XRMD_CMD,
	FTP_RMD_CMD,
	FTP_STRU_CMD,
	FTP_MODE_CMD,
	FTP_XMD5_CMD,
	FTP_LIST_CMD,
	FTP_NOOP_CMD,
	ESP_OPEN_CMD,
	FTP_SIZE_CMD,
	FTP_NULL_CMD,
}ftp_cmd;

enum ftp_type {
	ASCII_TYPE,
	IMAGE_TYPE,
	LOGICAL_TYPE
};

enum ftp_state {
	FTPS_NOT_LOGIN,
	FTPS_LOGIN,
	FTPS_DATAFLOW,
};

typedef enum{
	FTPS_SESSION_NONE,
	FTPS_SESSION_DATAPORT_OPEN,
	FTPS_SESSION_DATAPORT_CLOSE,
	FTPS_SESSION_FTP_OPEN,
	FTPS_SESSION_FTP_CLOSE,
}ftp_session_req;

typedef struct  {
	uint8_t control;			/* Control stream */
	uint8_t data;			/* Data stream */

	enum ftp_type type;		/* Transfer type */
	enum ftp_state state;
	uint8_t ip[4];
	/*FTP client IP address*/
  T32_8 client_ip;
  T16_8 client_port;
	char workingdir[512];
	char filename[512];
	char cmd[512];
  char arg[512];
	ftp_cmd current_cmd;	
	FIL fil;	// FatFs File objects
	FRESULT fr;	// FatFs function common result code
}FTPHandle_Typedef;


//Указатели на принятые пакеты
extern pESPPacket_Typedef *pReceivePackets[20];
extern int num_packet;
/********Prototype Functions************************************/

/**
 * Closes or Leave session depending on client request
 *
 * @param connection descriptor
 * @param buffer space 
 * @return 0 keep session, otherwise session needs to be closed
 */
uint8_t
FTP_QUIT_OR_WRONG_REQUEST(uint8_t *buf);



/**
 * Look if a file already exist with the name
 *  No case sensitive
 *
 * @param file name to look for on FAT
 * @return 0 if file was found, otherwise not
 */
uint8_t
FTP_Does_File_Exist(char *fname);

/**
 * Callback to send files name to ethernet
 *
 * @param descriptor to use for sending
 * @param filename string
 * @return always 1
 */
uint8_t
FTP_SD_send(void* var, uint8_t *fileName);

/**
 * Callback to check if file exists
 *
 * @param filename to compare
 * @param filename string
 * @return 0 if file found, otherwise zero
 */
uint8_t
FTP_CompareFile(void* var, uint8_t *fileName);

/**
 * Returns thru connection descriptor all files names in FAT
 *
 * @param connection descriptor to send files' names
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Send_NLST_List_Of_Files(char* path);
uint8_t
FTP_Send_List_Of_Files(char* path);
/**
 * Check if filename exists in FAT system for reading
 *
 * @param file name to check
 * @return 0 if read is possible, otherwise not
 */
uint8_t
FTP_Read_Is_Possible(char *fname);

/**
 * Gets a file from FAT and send it to a connection descriptor
 *  Call FTP_Read_Is_Possible(...)
 *
 * @param connection descriptor to write data read data
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Read_File(void);

/**
 * Puts a file from a connection descriptor and send it to FAT
 *  First check if file do not exist to avoid duplication of files in FAT
 *
 * @param connection descriptor to use read data
 * @param file name to write.
 * @return 0 if read was OK, otherwise not
 */
uint8_t
FTP_Write_File(uint8_t *path);

/**
 * Start an embedded FTP server Task: 1 client and 1 file per transfer
 *
 * @param paremeter pointer to use with task
 * @return none
 */
void
vBasicFTPServer( void *pvParameters );


void FTP_Init(void);
#endif
