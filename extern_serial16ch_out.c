#include <compiler.h>
#include <cfg/os.h>

#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>

#include <dev/board.h>
#include <dev/urom.h>
#include <dev/nplmmc.h>
#include <dev/sbimmc.h>
#include <fs/phatfs.h>

#include <sys/version.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/heap.h>
#include <sys/confnet.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/route.h>
#include <netdb.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/watchdog.h>
#include <sys/timer.h>
#include <dev/ds1307rtc.h>
#include <dev/rtc.h>
#include <time.h>
#include <dev/gpio.h>


#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bin_command_def.h"
#include "bsp.h"
#include "sysdef.h"
#include "cgi_thread.h"
#include "server.h"
#include "time_handle.h"
#include <dev/usartavr485.h>
#include <dev/relaycontrol.h>
#include <cfg/platform_def.h>
#include "io_out.h"
#include "sys_var.h"
#include "bsp.h"

#if 0
typedef struct _modbus_head
{
	uint8_t  pad0;
	uint8_t  pad1;
	uint8_t  group_addr;
	uint8_t  dev_addr;
	uint8_t  command;
	uint8_t  data_len;
} modbus_head;

typedef struct _relay_msk
{
	uint8_t ioary[2];
} st_relay_msk;


#define MODBUS_READ_RELAY        0
#define MODBUS_SET_RELAY         1
#define MODBUS_READ_INPUT        2
#define MODBUS_VERT_OUTPUT       3
#define MODBUS_SET_ONEBIT        4
#define MODBUS_CLR_ONTBIT        5
#define MODBUS_GET_ONEBIT        6
#define MODBUS_SET_BITMAP        7
#define MODBUS_CLR_BITMAP        8
#define MODBUS_SET_ADDRESS       9
#define MODBUS_GET_ADDRESS       10



#define MODBUS_DEVICE_PACKET     0
#define MODBUS_GROUP_PACKET      1


#endif

extern void dumpdata(void * _buffer,int len);

#ifdef   APP_485_ON

#define THISINFO       0
#define THISERROR      1


extern unsigned char io_out[32/8];


#if 0

uint8_t check_sum(uint8_t *buffer,uint8_t length)  
{
	uint8_t i;
	uint8_t sum = 0;
	for(i=0;i<length;i++) {
		sum += buffer[i];
	}
	return sum;
}
#endif

extern unsigned int CRC16(unsigned char *Array,unsigned int Len);

#if 0 //使用我的内部协议，不稳定
THREAD(ext16chioout_thread, arg)
{
	unsigned char tx_buffer[9];
	modbus_head  * pcmd = (modbus_head *)tx_buffer;
	st_relay_msk *  pst = (st_relay_msk *)&(tx_buffer[sizeof(modbus_head)]);
	//构造发送数据结构
	pcmd->pad0 = 0x00;
	pcmd->pad1 = 0x5A;
	pcmd->group_addr = 0;
	pcmd->dev_addr = 0;
	pcmd->command = MODBUS_SET_RELAY;
	pcmd->data_len = 2;
	NutThreadSetPriority(IO_EXTEND_THREAD_PRI);
	if(THISINFO)printf("ext16chioout_thread is running...\r\n");
	while(1)
	{
		if(sys_varient.iofile) {
			if(1) {
				//发送485数据
				if(sys_varient.stream_max485) {
					//构建发送程序
					//有一个BUG，也许是串口的....
					if(THISINFO)printf("ext io out 16 ch send data...\r\n");
					pst->ioary[0] = io_out[3]; //从16路开始
					pst->ioary[1] = io_out[2]; //从16路开始
					tx_buffer[8] = check_sum(tx_buffer,sizeof(tx_buffer)-1);
					fwrite(tx_buffer,sizeof(char),sizeof(tx_buffer),sys_varient.stream_max485);
				} else {
					if(THISERROR)printf("sys_io.stream_max485 not opened!\r\n");
				}
			} else {
				if(THISERROR)printf("sys_io.pioout == NULL error!\r\n");
			}
		} else {
			if(THISERROR)printf("sys_io.iofile not opened!\r\n");
		}
		if(THISINFO)printf("ext io out 16 ch wait 1s\r\n");
        //NutEventWait(&(sys_varient.io_out_event), 1000);
		NutSleep(50);
	}
}
#else  //后面修改为使用MODBUS_RTU协议，检错功能强些

struct modbus_rtu_force_multi_coils
{
	unsigned char slave_addr;
	unsigned char function_code;
	unsigned char start_addr_hi;
	unsigned char start_addr_lo;
	unsigned char quantity_coils_hi;
	unsigned char quantity_coils_lo;
	unsigned char byte_count;
	unsigned char force_data_1;
	unsigned char force_data_2;
	unsigned char crc_hi;
	unsigned char crc_lo;
};

#define FORCE_DATA_SIZE    2  //2个字节

THREAD(ext16chioout_thread, arg)
{
	unsigned char buffer[sizeof(struct modbus_rtu_force_multi_coils)];
	struct modbus_rtu_force_multi_coils  * pmodbus = (struct modbus_rtu_force_multi_coils *)buffer;
	//构造发送数据结构
	pmodbus->slave_addr = 247;
	pmodbus->function_code = 15;
	pmodbus->start_addr_hi = 0;
	pmodbus->start_addr_lo = 0;
	pmodbus->quantity_coils_hi = 0;
	pmodbus->quantity_coils_lo = 16;
	pmodbus->byte_count = 2;
	NutThreadSetPriority(IO_EXTEND_THREAD_PRI);
	if(THISINFO)printf("ext16chioout_thread is running...\r\n");
	while(1)
	{
		if(sys_varient.iofile) {
			if(1) {
				//发送485数据
				if(sys_varient.stream_max485) {
					unsigned int crc = 0;
					//构建发送程序
					//有一个BUG，也许是串口的....
					if(THISINFO)printf("ext io out 16 ch send data...\r\n");
					pmodbus->force_data_1 = io_out[3]; //从16路开始
					pmodbus->force_data_2 = io_out[2]; //从16路开始
					crc = CRC16(buffer,sizeof(buffer) - 2);
					pmodbus->crc_lo = crc >> 8;
					pmodbus->crc_hi = crc & 0xFF;
					fwrite(buffer,sizeof(char),sizeof(buffer),sys_varient.stream_max485);
				} else {
					if(THISERROR)printf("sys_io.stream_max485 not opened!\r\n");
				}
			} else {
				if(THISERROR)printf("sys_io.pioout == NULL error!\r\n");
			}
		} else {
			if(THISERROR)printf("sys_io.iofile not opened!\r\n");
		}
		if(THISINFO)printf("ext io out 16 ch wait 1s\r\n");
        NutEventWait(&(sys_varient.io_out_event), 50);
	}
}
#endif

void StartCAN_485Srever(void)
{
	uint32_t baud = 9600;

	if(THISINFO)printf("Start RS485 Server...\r\n");

    _ioctl(_fileno(sys_varient.stream_max485), UART_SETSPEED, &baud);
	baud = 5; //10ms
	_ioctl(_fileno(sys_varient.stream_max485), UART_SETREADTIMEOUT, &baud);
    NutThreadCreate("ext16chioout_thread", ext16chioout_thread, 0, 512);
}



#endif  //#ifdef   APP_485_ON