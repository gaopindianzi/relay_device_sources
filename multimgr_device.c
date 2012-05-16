
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
#include "modbus_interface.h"

#include "debug.h"

#define THISINFO        1
#define THISERROR       1
#define THISASSERT      1

#define   DEFAULT_WORK_PORT       505

//������豸��Ϣ
device_info_st    multimgr_info;
//
#define  PACKARY2_TOINT(buffer)    (((unsigned int)(buffer)[0])|(((unsigned int)(buffer)[1])<<8))


extern int  ReadCoilStatus(modbus_type_fc1_cmd * pmodbus);
extern int  ReadInputDiscretes(modbus_type_fc1_cmd * pmodbus);
extern int  ForceSingleCoil(modbus_type_fc5_cmd * pmodbus);
extern int force_multiple_coils(unsigned char * hexbuf,unsigned int len);

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
	if(pst->change_password) { //��ͬ����һ��ı�
		DEBUGMSG(THISINFO,("Change Password.\r\n"));
		memcpy(&multimgr_info,pst,sizeof(device_info_st));
	} else {//��������֮�⣬�������ı�
		DEBUGMSG(THISINFO,("Not Change Password.\r\n"));
		memcpy(pst->password,multimgr_info.password,sizeof(multimgr_info.password));
		memcpy(&multimgr_info,pst,sizeof(device_info_st));
	}
	DEBUGMSG(THISINFO,("Write MultiMgr Info.\r\n"));
	BspSavemultimgr_info(&multimgr_info);
	//�ϴ�,Ӧ��
	memset(pst->password,0,sizeof(pst->password));
	pst->to_host = 1;
	crc = CRC16((unsigned char *)pst,sizeof(device_info_st)-2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	NutUdpSendTo(socket,addr,port,pst,sizeof(multimgr_info));
}
void broadcast_itself(UDPSOCKET * socket,uint32_t host_addr,uint16_t port,unsigned char * buffer)
{
	unsigned int crc;
	device_info_st * pst = (device_info_st *)buffer;
	memcpy(pst,&multimgr_info,sizeof(multimgr_info));
	memset(pst->password,0,sizeof(multimgr_info.password));
	pst->change_password = 0;
	pst->command = CMD_SET_DEVICE_INFO;
	pst->command_len = sizeof(multimgr_info);
	pst->to_host = 1;
	pst->device_model = BOARD_TYPE_MODEL;
	crc = CRC16((unsigned char *)pst,sizeof(multimgr_info) - 2);
	pst->crc[0] = crc & 0xFF;
	pst->crc[1] = crc >> 8;
	DEBUGMSG(THISINFO,("Broadcast to port:%d,CRC(0x%X)\r\n",PACKARY2_TOINT(multimgr_info.work_port),crc));
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
			DEBUGMSG(THISINFO,("call CMD_MODBUSPACK_SEND.\r\n"));
			if(len < elen) {
				DEBUGMSG(THISERROR,("Prase Rx CMD_MODBUSPACK_SEND Data LEN ERROR!\r\n"));
				break;
			}
			dumpdata(mst,elen);
			crc = CRC16((unsigned char *)&mst->command_len,len - 3);
			if(mst->crc[0] != (unsigned char)(crc&0xFF) || mst->crc[1] != (unsigned char)(crc>>8)) {
				DEBUGMSG(THISERROR,("Prase Rx Data CMD_MODBUSPACK_SEND: Package CRC(0x%X) != mst->crc(0x%X%X) ERROR!\r\n",
					crc,mst->crc[0],mst->crc[1]));
				break;
			}
			//����modbusЭ��
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
							DEBUGMSG(THISINFO,("ReadCoilStatus Ok ret len(%d),CRC(0x%X)\r\n",mlen,crc));
							if(THISINFO)dumpdata(mst,mst->command_len);
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
					DEBUGMSG(THISINFO,("mosbus 0x0F func:\r\n"));
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
							dumpdata(mst,mst->command_len);
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
	//��ȡ����
	BspLoadmultimgr_info(&multimgr_info);
	if(gconfig&0x01) {
		//����ģʽ���޸Ĺ����Ķ˿ں�
		DEBUGMSG(THISINFO,("multimgr work in set mode\r\n"));
	    multimgr_info.work_port[0] = DEFAULT_WORK_PORT&0xFF;multimgr_info.work_port[1] = DEFAULT_WORK_PORT>>8;  //505
	    multimgr_info.broadcast_time = 3;
		multimgr_info.cncryption_mode = 0;
	} else {
		//����ģʽ������ָ����������
	}
	work_port = PACKARY2_TOINT(multimgr_info.work_port);
	//����һ��UDP
	DEBUGMSG(THISINFO,("MGR:Create UDP Port %d,%d\r\n",work_port,sizeof(device_info_st)));
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
		length = sizeof(rx_buffer);
		DEBUGMSG(THISINFO,("UDP Start RX(timeout=%d s)\r\n",multimgr_info.broadcast_time));
		int ret = NutUdpReceiveFrom(socket,&addr,&port,rx_buffer,length,((unsigned int)multimgr_info.broadcast_time)*1000);
		if(ret < 0) {
			DEBUGMSG(THISINFO,("UDP Receive error!\r\n"));
		} else if(ret == 0) {
			DEBUGMSG(THISINFO,("UDP RX Timeout boadcast itself.\n"));
			//�㲥�Լ�
			broadcast_itself(socket,0xFFFFFFFFUL,work_port,rx_buffer);
			//���涨�ĵ�ַ��������
			broadcast_itself(socket,0xFFFFFFFFUL,work_port,rx_buffer);
		} else {
		    DEBUGMSG(THISINFO,("UDP Receive %d bytes\r\n",ret));
			if(addr != 0xFFFFFFFFUL) { //�����ܹ㲥��
				prase_multimgr_rx_data(socket,addr,port,rx_buffer,ret);
			}
		}
	}
}


void StratMultiMgrDeviceThread(void)
{
	NutThreadCreate("multimgr_thread",  multimgr_thread, 0, 2024);
}
