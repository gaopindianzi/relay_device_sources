
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

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bsp.h"
#include "sysdef.h"

#include "des.h"
#include "multimgr_device.h"
#include "multimgr_device_dev.h"

#include "debug.h"

#define THISINFO        1
#define THISERROR       1
#define THISASSERT      1

#define   DEFAULT_WORK_PORT       505

//多管理设备信息
device_info_st    multimgr_info;
//
#define  PACKARY2_TOINT(buffer)    (((unsigned int)(buffer)[0])|(((unsigned int)(buffer)[1])<<8))


unsigned int CRC16(unsigned char *Array,unsigned int Len)
{
	unsigned int  IX,IY,CRC;
	CRC=0xFFFF;//set all 1
	if (Len<=0) {
		CRC = 0;
	} else {
		Len--;
		for (IX=0;IX<=Len;IX++)
		{
			CRC=CRC^(unsigned int)(Array[IX]);
			for(IY=0;IY<=7;IY++) {
				if ((CRC&1)!=0) {
					CRC=(CRC>>1)^0xA001;
				} else {
					CRC=CRC>>1;
				}
			}
		}
	}
	return CRC;
}

void dumpdata(void * _buffer,int len);


void UpdataMultiMgrDeviceInfo(UDPSOCKET * socket,uint32_t addr,uint16_t port,unsigned char * rx_data,int len)
{
	unsigned int crc;
	device_info_st * pst = (device_info_st *)rx_data;
	if(pst->change_password) { //连同密码一起改变
		DEBUGMSG(THISINFO,("Change Password.\r\n"));
		memcpy(&multimgr_info,pst,sizeof(device_info_st));
	} else {//除了密码之外，其他都改变
		DEBUGMSG(THISINFO,("Not Change Password.\r\n"));
		memcpy(pst->password,multimgr_info.password,sizeof(multimgr_info.password));
		memcpy(&multimgr_info,pst,sizeof(device_info_st));
	}
	DEBUGMSG(THISINFO,("Write MultiMgr Info.\r\n"));
	BspSavemultimgr_info(&multimgr_info);
	//上传,应答
	memset(pst->password,0,sizeof(pst->password));
	pst->to_host = 1;
	crc = CRC16(pst,sizeof(device_info_st)-2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	NutUdpSendTo(socket,addr,port,pst,sizeof(multimgr_info));
}
void broadcast_itself(UDPSOCKET * socket,unsigned char * buffer)
{
	unsigned int crc;
	device_info_st * pst = (device_info_st *)buffer;
	memcpy(pst,&multimgr_info,sizeof(multimgr_info));
	memset(pst->password,0,sizeof(multimgr_info.password));
	pst->change_password = 0;
	pst->command = CMD_SET_DEVICE_INFO;
	pst->command_len = sizeof(multimgr_info);
	pst->to_host = 1;
	crc = CRC16(pst,sizeof(multimgr_info) - 2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	DEBUGMSG(THISINFO,("Broadcast to port:%d,CRC(0x%X)\r\n",PACKARY2_TOINT(multimgr_info.work_port),crc));
	NutUdpSendTo(socket,0xFFFFFFFFUL,PACKARY2_TOINT(multimgr_info.work_port),(char*)&multimgr_info,sizeof(multimgr_info));
}

void prase_multimgr_rx_data(UDPSOCKET * socket,uint32_t addr,uint16_t port,const char * rx_data,int len)
{
	device_info_st * pst = (device_info_st *)rx_data;
	if(len < 3) {
		return ;
	}
	if(pst->to_host) {
		return ;
	}
	switch(pst->command)
	{
	case CMD_GET_DEVICE_INFO:
		{
		}
		break;
	case CMD_SET_DEVICE_INFO:
		{
			unsigned int crc;
			if(pst->command_len != sizeof(device_info_st) || len != sizeof(device_info_st)) {
				DEBUGMSG(THISERROR,("Prase Rx Set Device INFO Data LEN ERROR!\r\n"));
				break;
			}
			crc = CRC16(pst,sizeof(device_info_st) - 2);
			if(pst->crc[0] != (unsigned char)(crc&0xFF) || pst->crc[1] != (unsigned char)(crc>>8)) {
				DEBUGMSG(THISERROR,("Prase Rx Data: Package CRC(0x%X) ERROR!\r\n",crc));
				break;
			}
			UpdataMultiMgrDeviceInfo(socket,addr,port,rx_data,len);
		}
		break;
	default:
		break;
	}
}


THREAD(multimgr_thread, arg)
{
	unsigned int count = 0;
	int ret;
	uint16_t  work_port;
	unsigned char rx_buffer[512];
	uint16_t length = sizeof(rx_buffer);
	UDPSOCKET * socket = NULL;
	DEBUGMSG(THISINFO,("multimgr_thread is running...\r\n"));
    //NutThreadSetPriority(TCP_BIN_SERVER_PRI);
	//读取参数
	BspLoadmultimgr_info(&multimgr_info);
	BspLoadmultimgr_info(&multimgr_info);
	if(gconfig&0x01) {
		//设置模式，修改工作的端口号
		DEBUGMSG(THISINFO,("multimgr work in set mode\r\n"));
	    multimgr_info.work_port[0] = DEFAULT_WORK_PORT&0xFF;multimgr_info.work_port[1] = DEFAULT_WORK_PORT>>8;  //505
	    multimgr_info.broadcast_time = 10;
	} else {
		//工作模式，按照指定参数运行
	}
	work_port = PACKARY2_TOINT(multimgr_info.work_port);
	//创建一个UDP
	DEBUGMSG(THISINFO,("MGR:Create UDP Port %d,%d\r\n",work_port,sizeof(device_info_st)));
	socket = NutUdpCreateSocket(work_port);
	ASSERT(socket);
	//设置
	length = 1024;
	ret = NutUdpSetSockOpt(socket, SO_RCVBUF, &length, sizeof(length));
	ASSERT(ret==0);
	while(1) {
	    uint32_t addr = 0;
	    uint16_t port = 0;
	    //广播自己
		//往指定主机发消息
	    //然后等待数据，如果20s没有数据，重复广播自己，并往指定的主机发设备信息
		length = sizeof(rx_buffer);
		DEBUGMSG(THISINFO,("UDP Start RX(timeout=%d s)\r\n",multimgr_info.broadcast_time));
		int ret = NutUdpReceiveFrom(socket,&addr,&port,rx_buffer,length,((unsigned int)multimgr_info.broadcast_time)*1000);
		if(ret < 0) {
			DEBUGMSG(THISINFO,("UDP Receive error!\r\n"));
		} else if(ret == 0) {
			DEBUGMSG(THISINFO,("UDP RX Timeout boadcast itself.\n"));
			//超时
			//广播自己
			broadcast_itself(socket,rx_buffer);
		} else {
		    DEBUGMSG(THISINFO,("UDP Receive %d bytes\r\n",ret));
			if(addr != 0xFFFFFFFFUL) { //不接受广播包
				prase_multimgr_rx_data(socket,addr,port,rx_buffer,ret);
			}
		}
	}
}


void StratMultiMgrDeviceThread(void)
{
	NutThreadCreate("multimgr_thread",  multimgr_thread, 0, 1024);
}
