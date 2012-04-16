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
#include "debug.h"

#define THISINFO        1
#define THISERROR       0
#define THISASSERT      0
#define DUMP_DATA_INFO  1


#define BITS_TO_BS(bit_num)    ((bit_num+7)/8)


int  ReadCoilStatus(uint8_t * pmodbus,unsigned int start,unsigned int number)
{
	uint8_t io_out_buffer[4];
	uint32_t tmp;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	//DEBUGMSG(THISINFO,("iofile=0x%X\r\n",(unsigned int)iofile));
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_OUT_NUM, &tmp);
	ASSERT(tmp);
	rc = _ioctl(_fileno(iofile), IO_OUT_GET, io_out_buffer);
	fclose(iofile);
	ASSERT(rc==0);

	if(number) {
		//范围内
		unsigned char B,b;
		unsigned int i;
		B = start/8;b = start % 8;
		DEBUGMSG(THISINFO,("Read coil start=%d,number=%d\r\n",start,number));
		for(i=0;i<number;i++) {
			unsigned char msk = 1<<(i%8);
			pmodbus[1+i/8] <<= 1;
			if(B < BITS_TO_BS(tmp)) {
			    pmodbus[1+i/8] |= (io_out_buffer[B] & (1<<b))?msk:0;
			} else {
				pmodbus[1+i/8] &= ~msk;
			}
			if(++b >= 8) {
				b = 0;
				++B;
			}
		}
		//这里存放的是modbus/tcp数据的长度，等于字节的长度加上1
		pmodbus[0] = BITS_TO_BS(number); 
		//pmodbus[0] = (pmodbus[0]<2)?2:pmodbus[0];
		//pmodbus[0] += 1; //加上number_bytes的占位一个字节
		return 3 + pmodbus[0]; //返回发送的字节+head,1字节是数量位符,1字节是地址,1字节是功能码
	}
}

int  ForceSingleCoil(uint8_t * pmodbus,unsigned int start,unsigned char onoff)
{
	uint32_t tmp;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_OUT_NUM, &tmp);
	ASSERT(rc==0);
	{
		//范围内
		unsigned char buffer[2];
		buffer[1] = start >> 8;
		buffer[0] = start & 0xFF;
		if(onoff) {
		    _ioctl(_fileno(iofile), IO_SET_ONEBIT, buffer);
		} else {
			_ioctl(_fileno(iofile), IO_CLR_ONEBIT, buffer);
		}
		fclose(iofile);
		return 4;
	}
}

void dumpdata(void * _buffer,int len);

int prase_modbus_protocol(TCPSOCKET * sock,char * pbuf,unsigned int len)
{
	int ret = 0; //返回应答的数据长度
	modbus_tcp_head * phead = (modbus_tcp_head *)pbuf;
	uint8_t  * pmodbus = (uint8_t *)(pbuf+sizeof(modbus_tcp_head));

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
		{
			DEBUGMSG(THISINFO,("modbus 0x01:\r\n"));
			if(len < 4) {
				DEBUGMSG(THISERROR,("len < 4 error\r\n"));
				break;
			} else {
				unsigned int start,number;
				start = pmodbus[0]; start <<= 8; start |= pmodbus[1];
				number = pmodbus[2]; number <<= 8; number |= pmodbus[3];
				DEBUGMSG(THISINFO,("ReadCoilStatus\r\n"));
				len = ReadCoilStatus(pmodbus,start,number);
				if(len > 0) {
					DEBUGMSG(THISINFO,("ReadCoilStatus Ok len(%d)\r\n",len));
					//len += 1; //加上1字节的
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
				unsigned int start,onoff;
				start = pmodbus[0]; start <<= 8; start |= pmodbus[1];
				onoff = pmodbus[2];
				DEBUGMSG(THISINFO,("ForceSingleCoil\r\n"));
				len = ForceSingleCoil(pmodbus,start,onoff);
				if(len > 0) {
					DEBUGMSG(THISINFO,("ForceSingleCoil Ok,len(%d)\r\n",len));
					return len + sizeof(modbus_tcp_head);
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
				len = prase_modbus_protocol(sock,buff,len);
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