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

#define THISINFO        0
#define THISERROR       0
#define THISASSERT      0
#define DUMP_DATA_INFO  0

extern void dumpdata(void * _buffer,int len);

#define BITS_TO_BS(bit_num)    (((bit_num)+7)/8)

extern unsigned char io_out[32/8];
extern const uint32_t code_msk[32];

int  ReadCoilStatus(modbus_type_fc1_cmd * pmodbus)
{
	uint8_t io_out_buffer[4];
	uint16_t count;
	uint32_t tmp;
	uint16_t num;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_OUT_NUM, &tmp);
	ASSERT(tmp);
	num = tmp;
	rc = _ioctl(_fileno(iofile), IO_OUT_GET, io_out_buffer);
	fclose(iofile);
	ASSERT(rc==0);

	//if(THISINFO)dumpdata(pmodbus,sizeof(modbus_type_fc1_cmd));
	count = pmodbus->bit_count_h;
	count <<= 8; 
	count |= pmodbus->bit_count_l;
	DEBUGMSG(THISINFO,("read bit counts(0x%X,0x%X->0x%X)\r\n",pmodbus->bit_count_h,pmodbus->bit_count_l,count));
	
	modbus_type_fc1_ack * pack = (modbus_type_fc1_ack *)pmodbus;
	if(count) {
		unsigned char B,b;
		unsigned int i;
		tmp = pmodbus->ref_number_h; tmp <<=8; tmp |= pmodbus->ref_number_l;
		B = tmp / 8; b = tmp % 8;
		
		DEBUGMSG(THISINFO,("read bit counts(%d)\r\n",count));
		for(i=0;i<count;i++) {
			if(B < BITS_TO_BS(num)) {
				if(io_out_buffer[B]&(1<<b)) {
				    pack->bit_valus[i/8] |=  (1<<(i%8));
				} else {
					pack->bit_valus[i/8] &= ~(1<<(i%8));
				}
			} else {
				pack->bit_valus[i/8] &= ~(1<<(i%8));
			}
			if(++b >= 8) {
				b = 0;
				++B;
			}
		}
		pack->byte_count = BITS_TO_BS(count);
		//DEBUGMSG(THISINFO,("Read bit ok,return %d bytes\r\n",pack->byte_count));
		return 3 + pack->byte_count;  //����ͷ�������ݳ���
	} else {
		DEBUGMSG(THISERROR,("pmodbus->bit_count == 0 error\r\n"));
		pack->byte_count = 0;
		return 3;
	}
	return 0;
}

int  ReadInputDiscretes(modbus_type_fc1_cmd * pmodbus)
{
	uint8_t io_out_buffer[4];
	uint16_t count;
	uint32_t tmp;
	uint16_t num;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_IN_NUM, &tmp);
	ASSERT(tmp);
	num = tmp;
	rc = _ioctl(_fileno(iofile), IO_IN_GET, io_out_buffer);
	fclose(iofile);
	ASSERT(rc==0);
	count = pmodbus->bit_count_h;count <<= 8; count |= pmodbus->bit_count_l;
	modbus_type_fc1_ack * pack = (modbus_type_fc1_ack *)pmodbus;
	if(count) {
		unsigned char B,b;
		unsigned int i;
		tmp = pmodbus->ref_number_h; tmp <<=8; tmp |= pmodbus->ref_number_l;
		B = tmp / 8; b = tmp % 8;
		
		DEBUGMSG(THISINFO,("read bit counts(%d)\r\n",count));
		for(i=0;i<count;i++) {
			if(B < BITS_TO_BS(num)) {
				if(io_out_buffer[B]&(1<<b)) {
				    pack->bit_valus[i/8] |=  (1<<(i%8));
				} else {
					pack->bit_valus[i/8] &= ~(1<<(i%8));
				}
			} else {
				pack->bit_valus[i/8] &= ~(1<<(i%8));
			}
			if(++b >= 8) {
				b = 0;
				++B;
			}
		}
		pack->byte_count = BITS_TO_BS(count);
		return 3 + pack->byte_count;  //����ͷ�������ݳ���
	} else {
		DEBUGMSG(THISERROR,("pmodbus->bit_count == 0 error\r\n"));
		pack->byte_count = 0;
		return 3;
	}
	return 0;
}

int  ForceSingleCoil(modbus_type_fc5_cmd * pmodbus)
{
	uint32_t tmp;
	uint16_t index;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_OUT_NUM, &tmp);
	ASSERT(rc==0);
	index = pmodbus->ref_number_h; index <<= 8; index |= pmodbus->ref_number_l;
	if(index < tmp) {
		//��Χ��
		unsigned char buffer[2];
		buffer[1] = index >> 8;
		buffer[0] = index & 0xFF;
		if(pmodbus->onoff) {
		    _ioctl(_fileno(iofile), IO_SET_ONEBIT, buffer);
		} else {
			_ioctl(_fileno(iofile), IO_CLR_ONEBIT, buffer);
		}
	}
	fclose(iofile);
	return sizeof(modbus_type_fc5_cmd);
}


int force_multiple_coils(unsigned char * hexbuf,unsigned int len)
{
	unsigned int number,i;
	unsigned char bytes  = hexbuf[6];
	unsigned int address = hexbuf[2];

	uint32_t tmp;
	int rc;
	FILE * iofile = fopen("relayctl", "w+b");
	ASSERT(iofile);
	rc = _ioctl(_fileno(iofile), GET_OUT_NUM, &tmp);
	ASSERT(rc==0);
	fclose(iofile);

	address <<= 8;
	address |= hexbuf[3];
	number = hexbuf[4];
	number <<= 8;
	number |= hexbuf[5];
	if(((number+7)/8) != bytes) {
		DEBUGMSG(THISERROR,("force_multiple_coils data LEN ERROR!\r\n"));
		return 0;//���ܵ����ݳ��Ȳ���
	}
	DEBUGMSG(THISINFO,("force_multiple_coils stat(%d),len(%d) bytes\r\n",address,bytes));
	for(i=0;i<number;i++) {
		if(address < tmp) {
			//��ȡ������
			if(hexbuf[7+i/8]&code_msk[i%8]) {
				//���ü̵���
				io_out[address/8] |= code_msk[address%8];
			} else {
				io_out[address/8] &=~code_msk[address%8];
			}
		}
		address++;
	}
	return 6;
}


void dumpdata(void * _buffer,int len);

int prase_modbus_protocol(char * pbuf,unsigned int len)
{
	int ret = 0; //����Ӧ������ݳ���
	modbus_tcp_head * phead = (modbus_tcp_head *)pbuf;

	if(DUMP_DATA_INFO)dumpdata(pbuf,len);

	//�ж��Ƿ�modbus/tcpЭ��
	if(len < sizeof(modbus_tcp_head)) {
		DEBUGMSG(THISERROR,("modbus/tcp data len error!\r\n"));
		return 0;
	}
	//��ȡ���ݳ���
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
		tmp32 = 30000;  //��ʱ
		NutTcpSetSockOpt(sock,SO_RCVTIMEO,&tmp32,sizeof(uint32_t));
		if(NutTcpAccept(sock,502)) {
			//�ȴ����ճ�ʱ�����´�����
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
				//�������ԣ������߰ε��ٲ��ϣ�ֻ����ֳ�ʱ���쳣��ϵͳ��������⡣
				//���ԣ���ʱ��ʱ��Ҳ�����˳�����
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