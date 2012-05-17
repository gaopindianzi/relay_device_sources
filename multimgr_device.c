
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

#include <dev/reset_avr.h>

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
#include "modbus_interface.h"
#include "time_handle.h"

#include "debug.h"

#define THISINFO        1
#define THISERROR       1
#define THISASSERT      1

#define   DEFAULT_WORK_PORT       505

extern FILE * resetfile;

//多管理设备信息
device_info_st    multimgr_info;
//
#define  PACKARY2_TOINT(buffer)    (((unsigned int)(buffer)[0])|(((unsigned int)(buffer)[1])<<8))


extern int  ReadCoilStatus(modbus_type_fc1_cmd * pmodbus);
extern int  ReadInputDiscretes(modbus_type_fc1_cmd * pmodbus);
extern int  ForceSingleCoil(modbus_type_fc5_cmd * pmodbus);
extern int  force_multiple_coils(unsigned char * hexbuf,unsigned int len);

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
	device_info_st   newinfo;
	memcpy(&newinfo,pst,sizeof(device_info_st));
	if(!(pst->change_password)) {//不修改密码
		DEBUGMSG(THISINFO,("Not Change Password.\r\n"));
		memcpy(newinfo.password,multimgr_info.password,sizeof(multimgr_info.password));
	}
	if(!(pst->change_ipconfig)) { //
		DEBUGMSG(THISINFO,("NOT change ipconfig.\r\n"));
		memcpy(newinfo.local_ip,multimgr_info.local_ip,
			(sizeof(multimgr_info.local_ip)+sizeof(multimgr_info.net_mask)+sizeof(multimgr_info.gateway)+
			sizeof(multimgr_info.dns)));
	}
	DEBUGMSG(THISINFO,("Write MultiMgr Info.\r\n"));
	BspSavemultimgr_info(&newinfo);
	//上传,应答
	BspLoadmultimgr_info(pst);
	memset(pst->password,0,sizeof(pst->password));
	pst->to_host = 1;
	crc = CRC16((unsigned char *)pst,sizeof(device_info_st)-2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	NutUdpSendTo(socket,addr,port,pst,sizeof(multimgr_info));
}

#if 0
struct _tm {
    int tm_sec;                 /*!< \brief seconds after the minute - [0,59] */
    int tm_min;                 /*!< \brief minutes after the hour - [0,59] */
    int tm_hour;                /*!< \brief hours since midnight - [0,23] */
    int tm_mday;                /*!< \brief day of the month - [1,31] */
    int tm_mon;                 /*!< \brief months since January - [0,11] */
    int tm_year;                /*!< \brief years since 1900 */
    int tm_wday;                /*!< \brief days since Sunday - [0,6] */
    int tm_yday;                /*!< \brief days since January 1 - [0,365] */
    int tm_isdst;               /*!< \brief daylight savings time flag */
};
int raed_system_time_value(time_type * rio)
{
	NutMutexLock(&sys_time_lock);
	rio->sec = sys_time.tm_sec;
	rio->min = sys_time.tm_min;
	rio->hour = sys_time.tm_hour;
	rio->day = sys_time.tm_mday;
	rio->mon = sys_time.tm_mon;
	rio->year = sys_time.tm_year;
	NutMutexUnlock(&sys_time_lock);
	return 0;
}
typedef struct _time_type
{
	uint8_t    year;
	uint8_t    mon;
	uint8_t    day;
	uint8_t    hour;
	uint8_t    min;
	uint8_t    sec;
} time_type;
#endif

extern CONFNET confnet;

void broadcast_itself(UDPSOCKET * socket,uint32_t host_addr,uint16_t port,unsigned char * buffer)
{
	unsigned int crc;
	time_type time;
	device_info_st * pst = (device_info_st *)buffer;
	memcpy(pst,&multimgr_info,sizeof(multimgr_info));
	memset(pst->password,0,sizeof(multimgr_info.password));
	//设备时间
	raed_system_time_value(&time);
	pst->device_time[5] = time.year;
	pst->device_time[4] = time.mon;
	pst->device_time[3] = time.day;
	pst->device_time[2] = time.hour;
	pst->device_time[1] = time.min;
	pst->device_time[0] = time.sec;
	//MAC地址
	{
		//u_char de_mac[] = SYS_DEFAULT_MAC;
		memcpy(pst->mac,confnet.cdn_mac,6);
	}
	{//IP地址
		pst->local_ip[0] = (unsigned char)(confnet.cdn_ip_addr >> 24);
		pst->local_ip[1] = (unsigned char)(confnet.cdn_ip_addr >> 16);
		pst->local_ip[2] = (unsigned char)(confnet.cdn_ip_addr >> 8);
		pst->local_ip[3] = (unsigned char)(confnet.cdn_ip_addr >> 0);

		pst->net_mask[0] = (unsigned char)(confnet.cdn_ip_mask >> 24);
		pst->net_mask[1] = (unsigned char)(confnet.cdn_ip_mask >> 16);
		pst->net_mask[2] = (unsigned char)(confnet.cdn_ip_mask >> 8);
		pst->net_mask[3] = (unsigned char)(confnet.cdn_ip_mask >> 0);

		pst->gateway[0] = (unsigned char)(confnet.cdn_gateway >> 24);
		pst->gateway[1] = (unsigned char)(confnet.cdn_gateway >> 16);
		pst->gateway[2] = (unsigned char)(confnet.cdn_gateway >> 8);
		pst->gateway[3] = (unsigned char)(confnet.cdn_gateway >> 0);

		pst->dns[0] = (unsigned char)(confnet.cdn_cip_addr >> 24);
		pst->dns[1] = (unsigned char)(confnet.cdn_cip_addr >> 16);
		pst->dns[2] = (unsigned char)(confnet.cdn_cip_addr >> 8);
		pst->dns[3] = (unsigned char)(confnet.cdn_cip_addr >> 0);
	}
	//
	pst->change_password = 0;
	pst->command = CMD_SET_DEVICE_INFO;
	pst->command_len = sizeof(multimgr_info);
	pst->to_host = 1;
	pst->device_model = BOARD_TYPE_MODEL;
	crc = CRC16((unsigned char *)pst,sizeof(multimgr_info) - 2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	//DEBUGMSG(THISINFO,("Broadcast to port:%d,CRC(0x%X)\r\n",PACKARY2_TOINT(multimgr_info.work_port),crc));
	//dumpdata((const char *)&multimgr_info,sizeof(multimgr_info));
	NutUdpSendTo(socket,host_addr,port,(char*)pst,sizeof(multimgr_info));
}

void prase_multimgr_rx_data(UDPSOCKET * socket,uint32_t addr,uint16_t port,unsigned char * rx_data,int len)
{
	device_info_st * pst = (device_info_st *)rx_data;
	if(len < 3) {
		return ;
	}
	switch(pst->command)
	{
	case CMD_GET_DEVICE_INFO:
		{
			DEBUGMSG(THISINFO,("broadcast_itself to get device info command.\r\n"));
			broadcast_itself(socket,addr,port,rx_data);
		}
		break;
	case CMD_SET_DEVICE_INFO:
		{
			unsigned int crc;
			if(pst->to_host) {
				break;
			}
			if(pst->command_len != sizeof(device_info_st) || len != sizeof(device_info_st)) {
				DEBUGMSG(THISERROR,("Prase Rx CMD_SET_DEVICE_INFO Data LEN ERROR!\r\n"));
				break;
			}
			crc = CRC16((unsigned char *)pst,sizeof(device_info_st) - 2);
			if(pst->crc[0] != (unsigned char)(crc&0xFF) || pst->crc[1] != (unsigned char)(crc>>8)) {
				DEBUGMSG(THISERROR,("Prase Rx Data CMD_SET_DEVICE_INFO: Package CRC(0x%X) ERROR!\r\n",crc));
				break;
			}
			UpdataMultiMgrDeviceInfo(socket,addr,port,rx_data,len);
		}
		break;
	case CMD_MODBUSPACK_SEND:
		{
			unsigned int crc;
			const unsigned int elen = sizeof(modbus_command_st) + sizeof(modbus_tcp_head);
			modbus_command_st * mst = (modbus_command_st *)pst;
			modbus_tcp_head * mhead;
			//DEBUGMSG(THISINFO,("call CMD_MODBUSPACK_SEND.\r\n"));
			if(len < elen) {
				DEBUGMSG(THISERROR,("Prase Rx CMD_MODBUSPACK_SEND Data LEN ERROR!\r\n"));
				break;
			}
			//dumpdata(mst,elen);
			crc = CRC16((unsigned char *)&mst->command_len,len - 3);
			if(mst->crc[0] != (unsigned char)(crc&0xFF) || mst->crc[1] != (unsigned char)(crc>>8)) {
				DEBUGMSG(THISERROR,("Prase Rx Data CMD_MODBUSPACK_SEND: Package CRC(0x%X) != mst->crc(0x%X%X) ERROR!\r\n",
					crc,mst->crc[0],mst->crc[1]));
				break;
			}
			//解析modbus协议
			mhead = (modbus_tcp_head *)GET_MODBUS_COMMAND_DATA(mst);
			switch(mhead->function_code) 
			{
			case 0x01:
			case 0x02:
				{
					int mlen = mhead->lengthh;
					mlen <<= 8;
					mlen |= mhead->lengthl;
					//modbus_type_fc1_cmd * pcmd = (modbus_type_fc1_cmd *)GET_MODBUS_DATA(phead);
					DEBUGMSG(THISINFO,("modbus 0x01:\r\n"));
					if(mlen < 4) {
						DEBUGMSG(THISERROR,("len < 4 error\r\n"));
						break;
					} else {
						if(mhead->function_code == 0x01) {
							mlen = ReadCoilStatus(GET_MODBUS_DATA(mhead));
						} else if(mhead->function_code == 0x02) {
							mlen = ReadInputDiscretes(GET_MODBUS_DATA(mhead));
						}
						if(mlen > 0) {
							unsigned int crc;
							mhead->lengthl = mlen & 0xFF;
							mhead->lengthh = mlen >> 8;
							mst->command_len = sizeof(modbus_command_st) + sizeof(modbus_tcp_head) + mlen - 2;
							crc = CRC16((unsigned char *)&mst->command_len,mst->command_len - 3);
							mst->crc[0] = (unsigned char)(crc&0xFF);
							mst->crc[1] = (unsigned char)(crc>>8);
							//DEBUGMSG(THISINFO,("ReadCoilStatus Ok ret len(%d),CRC(0x%X)\r\n",mlen,crc));
							//if(THISINFO)dumpdata(mst,mst->command_len);
							NutUdpSendTo(socket,addr,port,mst,mst->command_len);
						} else {
							DEBUGMSG(THISERROR,("ReadCoilStatus Ko\r\n"));
						}
					}
				}
				break;
			case 0x05:
				{
					int ret;
					DEBUGMSG(THISINFO,("Call ForceSingleCoil.\r\n"));
					ret = ForceSingleCoil(GET_MODBUS_DATA(mhead));
					if(ret > 0) {
						NutUdpSendTo(socket,addr,port,mst,len);
					} else {
						DEBUGMSG(THISERROR,("ForceSingleCoil ERROR!\r\n"));
					}
				}
				break;
			case 0x0F:
				{
					//DEBUGMSG(THISINFO,("mosbus 0x0F func:\r\n"));
					if(len < 17) {
						DEBUGMSG(THISERROR,("modbus force_multiple_coils data len < 8 ERROR!\r\n"));
					} else {
						DEBUGMSG(THISINFO,("call force_multiple_coils.\r\n"));
						int mlen = force_multiple_coils(GET_MODBUS_DATA(mhead),len);
						if(mlen > 0) {
							unsigned int crc;
							mhead->lengthl = mlen & 0xFF;
							mhead->lengthh = mlen >> 8;
							mst->command_len = sizeof(modbus_command_st) + sizeof(modbus_tcp_head) + mlen - 2;
							crc = CRC16((unsigned char *)&mst->command_len,mst->command_len - 3);
							mst->crc[0] = (unsigned char)(crc&0xFF);
							mst->crc[1] = (unsigned char)(crc>>8);
							DEBUGMSG(THISINFO,("force_multiple_coils Ok ret len(%d),CRC(0x%X)\r\n",mlen,crc));
							//dumpdata(mst,mst->command_len);
							NutUdpSendTo(socket,addr,port,mst,mst->command_len);
						} else {
							DEBUGMSG(THISERROR,("force_multiple_coils Ko\r\n"));
						}
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	case CMD_RESET_DEVICE:
		{
			unsigned int crc;
			reset_device_st * rst = (reset_device_st *)rx_data;
			if(len != sizeof(reset_device_st)) {
				DEBUGMSG(THISINFO,("reset device data len ERROR1!\r\n"));
				break;
			}
			if(rst->command_len != len) {
				DEBUGMSG(THISINFO,("reset device data len ERROR2!\r\n"));
				break;
			}
			crc = CRC16((unsigned char *)rst,sizeof(reset_device_st) - 2);
			if(rst->crc[0] != (unsigned char)(crc&0xFF) || rst->crc[1] != (unsigned char)(crc>>8)) {
				DEBUGMSG(THISERROR,("Prase Rx CMD_RESET_DEVICE: Package CRC(0x%X) != rst->crc(0x%X%X) ERROR!\r\n",
					crc,rst->crc[1],rst->crc[0]));
				//dumpdata(rst,len);
				break;
			}
			//返回报文
			NutUdpSendTo(socket,addr,port,rst,len);
			//可以重启系统
			DEBUGMSG(THISINFO,("Reset System...\r\n"));
			_ioctl(_fileno(resetfile), SET_RESET, NULL);
		}
		break;
	default:
		break;
	}
}


THREAD(multimgr_thread, arg)
{
	unsigned int broadcasttime;
	char         cncryption_mode = 0;
	int ret;
	uint16_t  work_port;
	unsigned char rx_buffer[512];
	uint16_t length = sizeof(rx_buffer);
	UDPSOCKET * socket = NULL;
	DEBUGMSG(THISINFO,("multimgr_thread is running...\r\n"));
    //NutThreadSetPriority(TCP_BIN_SERVER_PRI);
	//读取参数
	BspLoadmultimgr_info(&multimgr_info);
	if(gconfig&0x01) {
		//设置模式，修改工作的端口号
		DEBUGMSG(THISINFO,("multimgr work in set mode\r\n"));
		work_port = DEFAULT_WORK_PORT;
		broadcasttime = 10;
	    cncryption_mode = 0;
	} else {
		//工作模式，按照指定参数运行
		DEBUGMSG(THISINFO,("multimgr work in normal mode\r\n"));
		work_port = PACKARY2_TOINT(multimgr_info.work_port);
		broadcasttime = multimgr_info.broadcast_time;
		cncryption_mode = multimgr_info.cncryption_mode;
	}
	broadcasttime = (broadcasttime < 2)?2:broadcasttime;
	//创建一个UDP
	DEBUGMSG(THISINFO,("MGR:Create UDP Port %d,%d\r\n",work_port,sizeof(device_info_st)));
	socket = NutUdpCreateSocket(work_port);
	ASSERT(socket);
	//设置
	length = 1024;
	ret = NutUdpSetSockOpt(socket, SO_RCVBUF, &length, sizeof(length));
	ASSERT(ret==0);
	NutSleep(3000);
	goto enter;
	while(1) {
	    uint32_t addr = 0;
	    uint16_t port = 0;
	    //广播自己
		//往指定主机发消息
	    //然后等待数据，如果20s没有数据，重复广播自己，并往指定的主机发设备信息
		length = sizeof(rx_buffer);
		DEBUGMSG(THISINFO,("UDP Start RX(timeout=%d s)\r\n",broadcasttime));
		int ret = NutUdpReceiveFrom(socket,&addr,&port,rx_buffer,length,((unsigned int)broadcasttime)*1000);
		if(ret < 0) {
			DEBUGMSG(THISINFO,("UDP Receive error!\r\n"));
		} else if(ret == 0) {
			//DEBUGMSG(THISINFO,("UDP RX Timeout boadcast itself.\n"));
enter:
			//广播自己
			DEBUGMSG(THISINFO,("broadcast_itself to 255.255.255.255.\r\n"));
			broadcast_itself(socket,0xFFFFFFFFUL,work_port,rx_buffer);
			//往规定的地址发送数据
			{
				uint32_t remote_addr;
				uint16_t remote_port = multimgr_info.remote_host_port[1];
				remote_port <<= 8;
				remote_port += multimgr_info.remote_host_port[0];
				multimgr_info.remote_host_addr[sizeof(multimgr_info.remote_host_addr) - 1] = 0;

				remote_addr = inet_addr(multimgr_info.remote_host_addr);
				if(remote_addr == -1) {
					if ((remote_addr = NutDnsGetHostByName((u_char*)multimgr_info.remote_host_addr)) != 0) {
						//成功解析
						if(THISINFO)printf("get host addr ip(%d.%d.%d.%d) ok!\r\n",
							(uint8_t)((remote_addr>>0)&0xFF),(uint8_t)((remote_addr>>8)&0xFF),(uint8_t)((remote_addr>>16)&0xFF),(uint8_t)((remote_addr>>24)&0xFF));
					} else {
						//不能成功解析IP地址
						//等待，继续解析
						if(THISERROR)printf("Cound not prase the %s to ip addr!,try again.\r\n",multimgr_info.remote_host_addr);
						//NutSleep(1000);
						continue;
					}
				} else {
					DEBUGMSG(THISINFO,("is ip addr,broad cast directect.\r\n"));
				}
				DEBUGMSG(THISINFO,("broadcast_itself to %s host:%d.\r\n",multimgr_info.remote_host_addr,remote_port));
				broadcast_itself(socket,remote_addr,remote_port,rx_buffer);
			}
		} else {
		    //DEBUGMSG(THISINFO,("UDP Receive %d bytes\r\n",ret));
			if(addr != 0xFFFFFFFFUL) { //不接受广播包
				prase_multimgr_rx_data(socket,addr,port,rx_buffer,ret);
			}
		}
	}
}


void StratMultiMgrDeviceThread(void)
{
	NutThreadCreate("multimgr_thread",  multimgr_thread, 0, 2024);
}
