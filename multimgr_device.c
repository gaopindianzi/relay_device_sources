
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

#include "debug.h"

#define THISINFO        1
#define THISASSERT      1

#define   DEFAULT_WORK_PORT       20191

//������豸��Ϣ
device_info_st    multimgr_info;
//
#define  PACKARY2_TOINT(buffer)    (((unsigned int)(buffer)[0])|(((unsigned int)(buffer)[1])<<8))

THREAD(multimgr_thread, arg)
{
	int ret;
	uint16_t  work_port;
	unsigned char rx_buffer[32];
	uint16_t length = sizeof(rx_buffer);
	UDPSOCKET * socket = NULL;
	DEBUGMSG(THISINFO,("multimgr_thread is running...\r\n"));
    //NutThreadSetPriority(TCP_BIN_SERVER_PRI);
	//��ȡ����
	if(gconfig&0x01) {
		//����ģʽ
		DEBUGMSG(THISINFO,("multimgr work in set mode\r\n"));
	    multimgr_info.work_port[0] = DEFAULT_WORK_PORT&0xFF;multimgr_info.work_port[1] = DEFAULT_WORK_PORT>>8;  //505
	    multimgr_info.broadcast_time = 1;
	} else {
		//����ģʽ
		DEBUGMSG(THISINFO,("multimgr work in normal mode\r\n"));
	    multimgr_info.work_port[0] = DEFAULT_WORK_PORT&0xFF;multimgr_info.work_port[1] = DEFAULT_WORK_PORT>>8;  //505
	    multimgr_info.broadcast_time = 1;
	}
	work_port = PACKARY2_TOINT(multimgr_info.work_port);
	//����һ��UDP
	DEBUGMSG(THISINFO,("MGR:Create UDP Port %d\r\n",work_port));
	socket = NutUdpCreateSocket(work_port);
	ASSERT(socket);
	//����
	length = 1024;
	ret = NutUdpSetSockOpt(socket, SO_RCVBUF, &length, sizeof(length));
	ASSERT(ret==0);
	while(1) {
	    uint32_t addr = 0;
	    uint16_t port = 0;
	    //�㲥�Լ�
		//��ָ����������Ϣ
	    //Ȼ��ȴ����ݣ����20sû�����ݣ��ظ��㲥�Լ�������ָ�����������豸��Ϣ
		int ret = NutUdpReceiveFrom(socket,&addr,&port,rx_buffer,length,((unsigned int)multimgr_info.broadcast_time)*1000);
		if(ret < 0) {
			DEBUGMSG(THISINFO,("UDP Receive error!\r\n"));
		} else if(ret == 0) {
			DEBUGMSG(THISINFO,("UDP Receive 0\r\n"));
			//��ʱ
			//�㲥�Լ�
			NutUdpSendTo(socket,0xFFFFFFFFUL,work_port,(char*)&multimgr_info,sizeof(multimgr_info));
			//
		} else {
		    DEBUGMSG(THISINFO,("UDP Receive %d bytes\r\n",ret));
		}
	}
}


void StratMultiMgrDeviceThread(void)
{
	NutThreadCreate("multimgr_thread",  multimgr_thread, 0, 1024);
}
