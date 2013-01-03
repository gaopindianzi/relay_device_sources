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
#include <errno.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>
#include <dev/reset_avr.h>
#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "modbus_interface.h"
#include <dev/relaycontrol.h>

#include "sysdef.h"
#include "bin_command_def.h"
#include "time_handle.h"
#include "io_out.h"
#include "io_time_ctl.h"
#include "bsp.h"
#include "sys_var.h"
#include "plc_prase.h"
#include "compiler.h"

#include "debug.h"

#define THISINFO        0
#define THISERROR       0
#define THISASSERT      0
#define DUMP_DATA_INFO  0

extern void dumpdata(void * _buffer,int len);

int  ReadCoilStatus(modbus_type_fc1_cmd * pmodbus)
{
	uint16_t startbits,count,i;

	modbus_type_fc1_ack * pack = (modbus_type_fc1_ack *)pmodbus;

	count = HSB_BYTES_TO_WORD(&pmodbus->bit_count_h),
	startbits  = HSB_BYTES_TO_WORD(&pmodbus->ref_number_h),

	memset(&pack->bit_valus[0],0,BITS_TO_BS(count));
	for(i=0;i<count;i++) {
		unsigned char bit = get_bitval(startbits+i);
		SET_BIT(&pack->bit_valus[0],i,bit);
	}

	pack->byte_count = BITS_TO_BS(count);	
	return 3 + pack->byte_count;  //加上头部的数据长度
}

int  ReadInputDiscretes(modbus_type_fc1_cmd * pmodbus)
{
	uint16_t count,startbit,i;
	modbus_type_fc1_ack * pack = (modbus_type_fc1_ack *)pmodbus;
	startbit = HSB_BYTES_TO_WORD(&pmodbus->ref_number_h);
	count = HSB_BYTES_TO_WORD(&pmodbus->bit_count_h);

	for(i=0;i<count;i++) {
		unsigned char bit = get_bitval(startbit+i);
		SET_BIT(&pack->bit_valus[0],i,bit);
	}
	return 3 + pack->byte_count;  //加上头部的数据长度
}

int  ForceSingleCoil(modbus_type_fc5_cmd * pmodbus)
{
	uint16_t index;

	index = HSB_BYTES_TO_WORD(&pmodbus->ref_number_h);
	if(pmodbus->onoff) {
		set_bitval(index,1);
	} else {
		set_bitval(index,0);
	}
	return sizeof(modbus_type_fc5_cmd);
}


int force_multiple_coils(unsigned char * hexbuf,unsigned int len)
{
	typedef struct _fmc_t
	{
		unsigned char slave_addr;
		unsigned char function;
		unsigned char start_ref_num_hi;
		unsigned char start_ref_num_lo;
		unsigned char bit_count_hi;
		unsigned char bit_count_lo;
		unsigned char byte_count;
		unsigned char data_base;
	} fmc_t;
	uint16_t count,startbit,i;
	fmc_t * pm = (fmc_t *)hexbuf;
	unsigned char * iobits = &pm->data_base;
	startbit = HSB_BYTES_TO_WORD(&pm->start_ref_num_hi);
	count = HSB_BYTES_TO_WORD(&pm->bit_count_hi);

	for(i=0;i<count;i++) {
		set_bitval(startbit+i,BIT_IS_SET(iobits,i));
	}
	return 6;  //返回6个字节，不能动
}


void dumpdata(void * _buffer,int len);

int prase_modbus_protocol(char * pbuf,unsigned int len)
{
	int ret = 0; //返回应答的数据长度
	modbus_tcp_head * phead = (modbus_tcp_head *)pbuf;

	if(DUMP_DATA_INFO)dumpdata(pbuf,len);

	//判断是否modbus/tcp协议
	if(len < sizeof(modbus_tcp_head)) {
		DEBUGMSG(THISERROR,("modbus/tcp data len error!\r\n"));
		return 0;
	}
	//获取数据长度
	len = phead->lengthh;len <<= 8;
	len |= phead->lengthl;
	switch(phead->function_code) {
		case 0x01:
		case 0x02:
		{
			//modbus_type_fc1_cmd * pcmd = (modbus_type_fc1_cmd *)GET_MODBUS_DATA(phead);
			DEBUGMSG(THISINFO,("modbus 0x01:\r\n"));
			if(len < 4) {
				DEBUGMSG(THISERROR,("len < 4 error\r\n"));
				break;
			} else {
				if(phead->function_code == 0x01) {
				    len = ReadCoilStatus(GET_MODBUS_DATA(phead));
				} else if(phead->function_code == 0x02) {
					len = ReadInputDiscretes(GET_MODBUS_DATA(phead));
				}
				if(len > 0) {
					DEBUGMSG(THISINFO,("ReadCoilStatus Ok ret len(%d)\r\n",len));
					phead->lengthl = len & 0xFF;
					phead->lengthh = len >> 8;
					return len + sizeof(modbus_tcp_head) - 2;
				} else {
					DEBUGMSG(THISERROR,("ReadCoilStatus Ko\r\n"));
				}
			}
		}
		break;
		case 0x05:
		{
			DEBUGMSG(THISINFO,("modbus 0x05:\r\n"));
			if(len < 4) {
				DEBUGMSG(THISERROR,("len error!\r\n"));
			} else {
				DEBUGMSG(THISINFO,("ForceSingleCoil\r\n"));
				len = ForceSingleCoil(GET_MODBUS_DATA(phead));
				if(len > 0) {
					DEBUGMSG(THISINFO,("ForceSingleCoil Ok,ret len(%d)\r\n",len));
					phead->lengthl = len & 0xFF;
					phead->lengthh = len >> 8;
					return len + sizeof(modbus_tcp_head) - 2;
				} else {
					DEBUGMSG(THISERROR,("ForceSingleCoil Ko\r\n"));
				}
			}
		}
		break;
		default:
		break;
	}
	return ret;
}




THREAD(modbus_thread, arg)
{
	TCPSOCKET * sock;
	uint32_t tmp32 = 16000;

	DEBUGMSG(THISINFO,("modbus thread is running...\r\n"));
    NutThreadSetPriority(TCP_BIN_SERVER_PRI-1);
	while(1) {
		sock = NutTcpCreateSocket();
		ASSERT(sock);
		tmp32 = 30000;  //超时
		NutTcpSetSockOpt(sock,SO_RCVTIMEO,&tmp32,sizeof(uint32_t));
		if(NutTcpAccept(sock,502)) {
			//等待接收超时，从新打开连接
			NutTcpCloseSocket(sock);
			DEBUGMSG(THISERROR,("NutTcpAccept(sock) failed!\r\n"));
			continue;
		}
		DEBUGMSG(THISINFO,("Get One Connectting...\r\n"));
		while(1) {
			char buff[128];
			int len = NutTcpReceive(sock,buff,sizeof(buff));
            if(len == 0) {
				DEBUGMSG(THISERROR,("Tcp Recieve timeout.\r\n"));
				//经过测试，把网线拔掉再插上，只会出现超时的异常，系统不会出问题。
				//所以，超时的时候，也必须退出连接
				break;
			} else if(len == -1) {
				int error = NutTcpError(sock);
				DEBUGMSG(THISERROR,("CMD:Tcp Receive ERROR(%d)\r\n",error));
				if(error == ENOTCONN)  {
					DEBUGMSG(THISERROR,("CMD:Socket is not connected,break connecting\r\n"));
					break;
				} else {
					DEBUGMSG(THISERROR,("CMD:Socket is unknow error,break connecting\r\n"));
					break;
				}
			} else if(len > 0) {
				DEBUGMSG(THISINFO,("\r\n---------------------Get One Tcp packet length(%d)--------------------------\r\n",len));
				len = prase_modbus_protocol(buff,len);
				if(len > 0) {
					//
					DEBUGMSG(THISINFO,("Ack Data:%d\r\n",len));
					if(DUMP_DATA_INFO)dumpdata(buff,len);
					NutTcpSend(sock,buff,len);
				}
			}
		}
		NutTcpCloseSocket(sock);
	}
	DEBUGMSG(THISINFO,("modbus thread is exit.\r\n"));
}

void StartModbus_Interface(void)
{
	NutThreadCreate("modbus_thread",  modbus_thread, 0, 2024);
}