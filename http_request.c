#include <stdio.h>
#include <io.h>
#include <sys/msg.h>
#include <cfg/arch.h>
#include <dev/board.h>
#include <sys/socket.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <arpa/inet.h>
#include <pro/discover.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <dev/relaycontrol.h>
#include <cfg/platform_def.h>
///#include "http_server.h"
#include <dev/reset_avr.h>
#include <dev/relaycontrol.h>
#include <dev/watchdog.h>
#include <sys/timer.h>

#include "StringPrecess.h"
//#include "sysdef.h"

#include "bin_command_def.h"
#include "io_time_ctl.h"
//#include "cgi_thread.h"
#include "multimgr_device_dev.h"
#include "sys_var.h"
//#include "bsp.h"
#include "debug.h"

#define THISINFO          0
#define THISERROR         0
#define THISASSERT        0

#define HTTP_INFO         0
#define HTTP_DATA_PRINT   0


char http_buffer[1024];
char buffer[1024];
unsigned int rx_index = 0;


#ifdef APP_MULTI_MANGER_FRAME
extern void BspLoadmultimgr_info(device_info_st * info);
extern void BspSavemultimgr_info(device_info_st * info);
#endif

#ifdef APP_HTTP_PROTOTOL_CLIENT
extern int load_relay_info(ethernet_relay_info * info);
extern int save_relay_info(ethernet_relay_info * info);
#endif



void dump_data(const char * pbuf,int len)
{
	int i;
	for(i=0;i<len;i++) {
		putchar(*pbuf++);
	}
}

/*
I will send the request http data:

mac=23223322232AAEF
id=554333A224344
name=SCALL_A104_22
input_io_value=110010101010
output_io_value=1100101011001010101
index=xxxxx

i hope the response data is:
<body>
  <mac>23223322232AAEF</mac>
  <id>554333A224344</id>
  <set_relay_new_val>10101010101010010101</set_relay_new_val>
</body>
*/


#define HTTP_HEAD  "POST /%s HTTP/1.1\r\nHost: %s:%d\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n"

//extern u_char ethernet_mac[];

int HttpSendRequest(TCPSOCKET * sock)
{
	device_info_st   devinfo;
	int len = 0;
	uint32_t val = 0;
	int io_out_num = 0;
	//
	BspLoadmultimgr_info(&devinfo);

	http_buffer[0] = '\0'; //开始添加字符
	//StringAddStringRight(http_buffer,"ipaddr=");
	//StringAddStringRight(http_buffer,sys_info.local_ip);
	//StringAddStringRight(http_buffer,"mac=");
	//StringAddStringRight(http_buffer,sys_info.mac);
	StringAddStringRight(http_buffer,"id=");
	StringAddStringRight(http_buffer,sys_info.id);

	StringAddStringRight(http_buffer,"&name=");
	StringAddStringRight(http_buffer,devinfo.host_name);

	//StringAddStringRight(http_buffer,"&password=");
	//StringAddStringRight(http_buffer,sys_info.remote_password);

	StringAddStringRight(http_buffer,"&updata_period=");
	ValueIntToStringDec(buffer,sys_info.up_time_interval);
	StringAddStringRight(http_buffer,buffer);
	//
	StringAddStringRight(http_buffer,"&input_io_number=");
	_ioctl(_fileno(sys_varient.iofile), GET_IN_NUM,buffer);
	val = buffer[1]; 
	val <<= 8; 
	val |= buffer[0];
	if(HTTP_INFO)printf("GetIoInNumber:0x%X\r\n",(unsigned int)val);
	ValueIntToStringDec(buffer,val);
	StringAddStringRight(http_buffer,buffer);
	//
	StringAddStringRight(http_buffer,"&input_io_value=");
	_ioctl(_fileno(sys_varient.iofile), IO_IN_GET,buffer);
	val = buffer[1]; 
	val <<= 8; 
	val |= buffer[0];
	if(HTTP_INFO)printf("GetIoIn:0x%X\r\n",(unsigned int)val);
	ValueIntToStringBin(buffer,val);
	StringAddStringRight(http_buffer,buffer);
	//
	StringAddStringRight(http_buffer,"&output_io_number=");
	_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM,buffer);
	val = buffer[1]; 
	val <<= 8; 
	val |= buffer[0];
	io_out_num = val;
	if(HTTP_INFO)printf("GetIoOutNumber:0x%X\r\n",(unsigned int)val);
	ValueIntToStringDec(buffer,val);
	StringAddStringRight(http_buffer,buffer);
	//
	StringAddStringRight(http_buffer,"&output_io_value=");
	_ioctl(_fileno(sys_varient.iofile), IO_OUT_GET,buffer);
	val = buffer[1]; 
	val <<= 8; 
	val |= buffer[0];


	if(io_out_num == 2) {
		val &= 0x3;
	} else if(io_out_num == 4) {
		val &= 0xF;
	} else if(io_out_num == 8) {
		val &= 0xFF;
	} else if(io_out_num == 16) {
		val &= 0xFFFF;
	}

	if(HTTP_INFO)printf("GetIoOut:0x%X\r\n",(unsigned int)val);

	ValueIntToStringBin(buffer,val);

	StringAddStringRight(http_buffer,buffer);
	StringAddStringRight(http_buffer,"&rand=");
	val = rand();
	ValueIntToStringDec(buffer,val);
	StringAddStringRight(http_buffer,buffer);
	StringAddStringRight(http_buffer,"\r\n");
	
	len = strlen(http_buffer);
	sprintf(buffer,HTTP_HEAD,sys_info.web_page,sys_info.host_addr,sys_info.port,len);
	StringAddStringLeft(http_buffer,buffer);
	if(HTTP_DATA_PRINT) {
	    printf("\r\n\r\n\r\n\r\n------------------------------------------------------------------------------------\r\n");
	    printf("%s",http_buffer);
	}
	NutTcpSend(sock,http_buffer,strlen(http_buffer));
	NutSleep(1);
	return 0;
}

int HttpPrecessRxData(void)
{
	unsigned int st,end = 0;
	if(StringFindString(http_buffer,"<body>",&st)) {
		if(HTTP_INFO)printf("found string of index:%d\r\n",st);
		//从后续的字符串中继续找
		if(StringFindString(&http_buffer[st],"</body>",&end)) {
			//if(HTTP_INFO)printf("found string of index:%d\r\n",end+st);
			if(HTTP_INFO)dump_data(&http_buffer[st],end);
			if(StringFindString(&http_buffer[st],"<set_relay_new_val>",&st)) {
				if(StringFindString(&http_buffer[st],"</set_relay_new_val>",&end)) {
					unsigned int tmp = strlen("<set_relay_new_val>");
					StringSubString(&http_buffer[st],buffer,tmp,end-tmp);
					tmp = StringBinToValueInt(buffer);
					if(HTTP_INFO)printf("http_set_relay:0x%X\r\n",tmp);
					buffer[0] = tmp & 0xFF;
				    buffer[1] = (tmp >> 8) & 0xFF;
				    _ioctl(_fileno(sys_varient.iofile), IO_OUT_SET, buffer);
					return 0;
				}
			}
		}
	}
	return -1;
}


THREAD(tcp_client, arg)
{
	int try_count = 0;
	int ret = -1;
	uint32_t tmp;
	TCPSOCKET * socket = NULL;

    NutThreadSetPriority(102);

	DEBUGMSG(THISINFO,("php tcp client start...\r\n"));

	//获取复位信息

	while(1) {
		uint32_t              ip_addr;

		if(!sys_info.enable) {
			if(THISINFO)puts("waitting open client.");
			NutSleep(1000);
			continue;
		}

		ip_addr = inet_addr(sys_info.host_addr);

		if(++try_count > 3) {
			if(THISERROR)printf("\r\nRequestSystemReboot\r\n");
			NutSleep(5000);
			try_count = 0;
			continue;
		}

		if(ip_addr == -1) {
			//不是IP地址，起码前面xxx.xxx.xxx.xxx不符合IP地址规则
			//用DNS获取IP地址
		    if ((ip_addr = NutDnsGetHostByName((u_char*)sys_info.host_addr)) != 0) {
				//成功解析
				if(THISINFO)printf("get host addr ip(%d.%d.%d.%d) ok!\r\n",
					(uint8_t)((ip_addr>>0)&0xFF),(uint8_t)((ip_addr>>8)&0xFF),(uint8_t)((ip_addr>>16)&0xFF),(uint8_t)((ip_addr>>24)&0xFF));
			} else {
				//不能成功解析IP地址
				//等待，继续解析
				if(THISERROR)printf("Cound not prase the %s to ip addr!,try again.\r\n",sys_info.host_addr);
				NutSleep(1000);
				continue;
			}
		} else {
			if(THISINFO)printf("the host addr ip(%d.%d.%d.%d) ok!\r\n",
					(uint8_t)((ip_addr>>0)&0xFF),(uint8_t)((ip_addr>>8)&0xFF),(uint8_t)((ip_addr>>16)&0xFF),(uint8_t)((ip_addr>>24)&0xFF));
		}
		while(1) {
			int timeout_count = 0;

			if(!sys_info.enable) {
				if(THISINFO)puts("user close client.");
				break;
			}
		    //创建连接
			DEBUGMSG(THISINFO,("Nut Tcp Clreate Socket\r\n"));
		    socket = NutTcpCreateSocket();
			if(socket == 0) {
				DEBUGMSG(THISERROR,("Nut Tcp Create Socket failed,try again...!\r\n"));
				NutSleep(1000);
				continue;
			}
		    tmp = 60000;
		    ret = NutTcpSetSockOpt((TCPSOCKET *)socket,SO_RCVTIMEO,&tmp,sizeof(uint32_t));
		    ASSERT(!ret);
		    //连接
		    if(NutTcpConnect(socket,ip_addr,sys_info.port)) {
			    //不成功
				if(THISERROR)printf("NutTcpConnect(%s:%d) failed!,clsoe socket.\r\n",sys_info.host_addr,sys_info.port);
			    NutTcpCloseSocket(socket);
			    break;
		    }
			if(THISINFO)printf("tcp connect successful,send http request:\r\n");
		    //发送请求
		    //NutTcpSend
			HttpSendRequest(socket);
			if(THISINFO)printf("tcp start receive http response:\r\n");
			if(HTTP_DATA_PRINT) {
			    printf("------------------------------------------------------------------------------------\r\n");
			}
		    tmp = 10000;
		    ret = NutTcpSetSockOpt((TCPSOCKET *)socket,SO_RCVTIMEO,&tmp,sizeof(uint32_t));
			rx_index = 0;
			while(1) {
			    int len;
				if(sizeof(http_buffer) <= (rx_index + 1)) {
					rx_index = sizeof(http_buffer) - 1;
				}
				len = NutTcpReceive(socket,&http_buffer[rx_index],sizeof(http_buffer)-rx_index);
                if(len == 0) {
				    if(THISERROR)printf("Tcp Recieve timeout.\r\n");
					if(++timeout_count > 2) {
						break;
					}
			    } else if(len == -1) {
				    int error = NutTcpError(socket);
				    if(THISERROR)printf("CMD:Tcp Receive ERROR(%d)\r\n",error);
				    if(error == ENOTCONN)  {
					    if(THISERROR)printf("CMD:Socket is not connected,break connecting\r\n");
				    } else {
					    if(THISERROR)printf("CMD:Socket is unknow error,break connecting\r\n");
				    }
					break;
			   } else if(len > 0) {
				   //做处理
				   try_count = 0;
				   rx_index += len;
				   DEBUGMSG(THISINFO,("RX:%u\r\n",rx_index));
				   if(HTTP_DATA_PRINT)dump_data(http_buffer,rx_index);
				   if(HttpPrecessRxData() == 0) {
					   rx_index = 0;
					   break;
				   }
			    }
			}
		    //关闭
		    if(THISINFO)printf("tcp close socket.\r\n");
			NutTcpCloseSocket(socket);
			if(THISINFO)printf("NutSleep(%d)S\r\n",sys_info.up_time_interval);
			if(sys_info.up_time_interval == 0) {
				NutSleep(1000);
			} else {
				NutSleep(sys_info.up_time_interval*1000);
			}
		}
    }
}


void StartHttpRequestThread(void)
{
	NutThreadCreate("tcp_client", tcp_client, 0, 1048);
}