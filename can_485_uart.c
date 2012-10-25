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


extern void dumpdata(void * _buffer,int len);

#ifdef   APP_485_ON

#define THISINFO       0
#define THISERROR      0

#define     BUFFER_SIZE                  (128+10) 
uint8_t     socket_rx_buffer[BUFFER_SIZE];
uint8_t     rs485_rx_buffer[BUFFER_SIZE];


#define LITTLE_UINT16(buffer)   (((uint16_t)(*((unsigned char *)(buffer)))) + (((uint16_t)(*(((unsigned char *)(buffer))+1)))<<8))


uint8_t check_sum(uint8_t *buffer,uint8_t length)  
{
	uint8_t i;
	uint8_t sum = 0;
	for(i=0;i<length;i++) {
		sum += buffer[i];
	}
	return sum;
}

/*
-Modbus协议，
-buffer[0] = 0x00
-buffer[1] = 0x5A
-buffer[2] = 设备组地址
-buffer[3] = 设备节点地址
-buffer[4] = 命令
-buffer[5] = 数据长度
-buffer[6] = 数据1
-buffer[7] = 数据2
-buffer[8] = 校验和
*/



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

const unsigned char modbus_frame_size = sizeof(modbus_head)+sizeof(st_relay_msk)+1; //1字节是校验和长度

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

int Modbus_Command_Prase(unsigned char * buffer,unsigned char len,unsigned char flag)
{
	int rc = -1;
	unsigned char  send[modbus_frame_size];  
	modbus_head  * pcmd = (modbus_head *)buffer;
	modbus_head  * scmd = (modbus_head *)send;
	st_relay_msk *  pst = (st_relay_msk *)&(send[sizeof(modbus_head)]);

	if(pcmd->data_len != sizeof(st_relay_msk)) {
		if(THISERROR)printf("modbus data len Err!\r\n");
		return -1;
	}
	//初始化应答头和接受头相等
	memcpy(scmd,buffer,modbus_frame_size);
	//组播或者单播都符合的指令
	switch(pcmd->command) {
		case MODBUS_SET_RELAY:
		{
		    rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_SET, pst->ioary);
		}
		break;
    	case MODBUS_SET_ONEBIT:
		{
			rc = _ioctl(_fileno(sys_varient.iofile), IO_SET_ONEBIT, pst->ioary);
			rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
	    }
	    break;
	    case MODBUS_CLR_ONTBIT:
	    {
			rc = _ioctl(_fileno(sys_varient.iofile), IO_CLR_ONEBIT, pst->ioary);
			rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
		}
		break;
	    case MODBUS_VERT_OUTPUT:
	    {
			rc = _ioctl(_fileno(sys_varient.iofile), IO_SIG_BITMAP, pst->ioary);
			rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
		}
	    break;
		case MODBUS_SET_BITMAP:
		{
			rc = _ioctl(_fileno(sys_varient.iofile), IO_SET_BITMAP, pst->ioary);
			rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
		}
		break;
		case MODBUS_CLR_BITMAP:
		{
			rc = _ioctl(_fileno(sys_varient.iofile), IO_CLR_BITMAP, pst->ioary);
			rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
		}
		break;
		case MODBUS_READ_RELAY:
		if(flag == MODBUS_DEVICE_PACKET) {
		    scmd->data_len = 2;
		    rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, pst->ioary);
		}
		break;
	    case MODBUS_READ_INPUT:
	    if(flag == MODBUS_DEVICE_PACKET) {
			scmd->data_len = 2;
			rc = _ioctl(_fileno(sys_varient.iofile), IO_IN_GET, pst->ioary);
		}
		break;
		case MODBUS_GET_ONEBIT:
		if(flag == MODBUS_DEVICE_PACKET) {
			scmd->data_len = 2;
			rc = _ioctl(_fileno(sys_varient.iofile), IO_GET_ONEBIT, pst->ioary);
		}
		break;
		case MODBUS_SET_ADDRESS:
		if(flag == MODBUS_DEVICE_PACKET) {
			uint16_t addr = pst->ioary[0]; //组地址
			addr <<= 8;
			addr  |= pst->ioary[1]; //设备地址
			if(addr != 0x0000 || addr != 0xFFFF) { //保留地址，不允许写
				BspWriteEepromSerialAddress(addr);
			}
			addr = BspReadEepromSerialAddress();
			pst->ioary[0] = addr >> 8;
			pst->ioary[1] = addr & 0xFF;
			rc = 0;
		}
		break;
		case MODBUS_GET_ADDRESS:
		if(flag == MODBUS_DEVICE_PACKET) {
			uint16_t addr = BspReadEepromSerialAddress();
			pst->ioary[0] = addr >> 8;
			pst->ioary[1] = addr & 0xFF;
			rc = 0;
		}
		break;
		default:
		{
			if(THISERROR)printf("ERROR:modbus invalid command!\r\n");
		}
		break;
	}
	if(flag == MODBUS_DEVICE_PACKET && rc == 0) {
		scmd->pad1 = 0x55;
		send[sizeof(send)-1] = check_sum(send,sizeof(send)-1);
		rc = fwrite(send,sizeof(char),sizeof(send),sys_varient.stream_max485);
	}
	return rc;
}

THREAD(thread_can485_read, arg)
{
	uint8_t  group_addr  = 0;
	uint8_t  device_addr = 0;
	size_t  index = 0;
	modbus_head   * pcmd = (modbus_head *)rs485_rx_buffer;

	if(IoGetConfig()&(1<<0)) {
		if(THISINFO)printf("Can 485 Run On Setting mode,addr(0x%d,%d)\r\n",group_addr,device_addr);
	} else {
		uint16_t addr = BspReadEepromSerialAddress();
		if(addr == 0x0000 || addr == 0xFFFF) { //这太离谱了,占用特殊地址
			addr = 0xFFFE;
			BspWriteEepromSerialAddress(addr);
		}
		group_addr  = (unsigned char)(addr>>8);
		device_addr = (unsigned char)(addr&0xFF);
		if(THISINFO)printf("Can 485 Run On User mode,addr(0x%d,%d)\r\n",group_addr,device_addr);
	}

	//uint32_t st,dis;
	NutThreadSetPriority(TCP_BIN_SERVER_PRI + 1);

	if(THISINFO)printf("start rs485 loop!\r\n");
	while(1) 
	{
		unsigned char * pbuf = &rs485_rx_buffer[index];
		
		size_t size = sizeof(rs485_rx_buffer) - index;
		//if(THISINFO)printf("s r size = %d",index);
		size = fread(pbuf,sizeof(char),size,sys_varient.stream_max485);
		//if(THISINFO)printf("e r size = %d",size);
		if(size > 0) {
			//有数据
			index += size;
			//!!!!!!!!!!!!!!!!!!!!!!!!一下部分可能是BUG
			if(index >= 1) { //去除没用的头干扰信号，用于同步有用的信号
				if(pcmd->pad0 != 0x00) {
					goto pack_error;
				}
			} else if(index >= 2) { //去除没用的头干扰信号，用于同步有用的信号
				if(pcmd->pad0 != 0x00 || pcmd->pad1 != 0x5A) {
					goto pack_error;
				}
			}
			//如果接收到足够的数据
			if(index >= modbus_frame_size) {
				index = modbus_frame_size;
				goto handle_one_modbus_packet;
			}
		} else {
			//没有数据，超时
			if(index > 0) {
				//结束读
				if(index < modbus_frame_size) {
					//错误包，丢弃
					//if(THISERROR)printf("packet less than sizeof(modbus_head) size ERROR!\r\n");
					//太多干扰，太多错误,工频干扰严重，缆线很差
					goto pack_error;
				} else if(index > modbus_frame_size) {
					index = modbus_frame_size;
				}
handle_one_modbus_packet:
				//执行指令
				if(THISINFO)dumpdata(rs485_rx_buffer,index);

				if(pcmd->pad0 != 0x00 || pcmd->pad1 != 0x5A) {
					if(THISERROR)printf("0x00 0x5A Not Found!\r\n");
				}
				//CRC校验
				if(rs485_rx_buffer[index-1] != check_sum(rs485_rx_buffer,index-1)) {
					if(THISERROR)printf("packet check_sum Err(0x%X)!\r\n",check_sum(rs485_rx_buffer,index-1));
					goto pack_error;
				}
				if(pcmd->group_addr == group_addr && pcmd->dev_addr == device_addr) {//节点包
					Modbus_Command_Prase(rs485_rx_buffer,index,MODBUS_DEVICE_PACKET);
				} else if(pcmd->group_addr == 0xFF && pcmd->dev_addr == 0xFF) { //组播包
					Modbus_Command_Prase(rs485_rx_buffer,index,MODBUS_GROUP_PACKET);
				} else {
					if(THISERROR)printf("Not This Device Packet!\r\n");
				}
				//应答
				//
				goto command_finished;
			} else {
				//空读
				//if(THISINFO)printf("E");
			}
		}
		//BspDebugLedSet(led++);
		continue;
pack_error:
command_finished:
		//BspDebugLedSet(led++);
		index = 0;
	}
}

void StartCAN_485Srever(void)
{
	uint32_t baud = 9600;

	if(THISINFO)printf("Start RS485 Server...\r\n");

    _ioctl(_fileno(sys_varient.stream_max485), UART_SETSPEED, &baud);
	baud = 5; //10ms
	_ioctl(_fileno(sys_varient.stream_max485), UART_SETREADTIMEOUT, &baud);
    NutThreadCreate("thread_can485_read",  thread_can485_read, 0, 512);
}










#endif // #ifdef   APP_485_ON

