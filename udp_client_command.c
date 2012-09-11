
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

#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>
#include <netdb.h>
#include <pro/dhcp.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "sysdef.h"
#include "bin_command_def.h"
#include "time_handle.h"
#include "io_out.h"
#include "io_time_ctl.h"
#include "udp_client_command.h"
#include "bsp.h"


#define THISINFO       0
#define THISERROR      0

#ifdef USE_AUTO_CONFIG

typedef struct _MUSTER_TELE {
	//��ϢID��Ĭ��Ϊ4��0��
	uint32_t   xid;


	//��Ϣ���ͣ�0-��λ�������������ٻ��豸��1-�豸��Ӧ�ٻ����豸���أ���3-��λ�����豸������������
	uint8_t   msg_type;


	//muster�汾��һֱΪ1��
	uint8_t   muster_ver;


	//��������msg_type=0ʱ��0��msg_type=1ʱΪ�豸���ص���������msg_type=3ʱָ���豸������������
	uint8_t   net_hostname[12];


	//���������ַ��msg_type=0ʱ��0��msg_type=1ʱΪ�豸���ص�MAC��ַ��
	//msg_type=3ʱ��ָ��ΪҪ�޸��豸��Mac��ַ��
	uint8_t   net_mac[6];


	//IP��ַ��msg_type=0ʱ��0��msg_type=1ʱΪ�豸���ص�IP��ַ��msg_type=3ʱ�豸�޸�IPΪ�õ�ַ��
	uint32_t  net_ip_addr;


	//���������룬msg_type=0ʱ��0��msg_type=1ʱΪ�豸���ص����룬msg_type=3ʱ�豸�޸�����Ϊ�����롣
	uint32_t  net_ip_mask;


	//���أ�������������δʹ�á�
	uint32_t  net_gateway;


	//��Ʒ�ͺ�,��δʹ�á�
	uint32_t  devmodel;


	//����ģʽ,0-��Ч��1-Serverģʽ,2-Clientģʽ,10-UDPģʽ
	//msg_type=0ʱ��0��msg_type=1ʱΪ�豸���صĹ���ģʽ��msg_type=3ʱ�豸���Ըò�����
	uint32_t  workmodel;


	//�豸�Ķ˿�����msg_type=0ʱ��0��msg_type=1ʱΪ�豸���صĶ˿�������msg_type=3ʱ�豸���Ըò�����
	uint32_t  portnum;


	//��һ�����������������������δʹ��
	uint32_t  firstport;


	//�ı���ʽ�Ĺ̼��汾˵����
	//msg_type=0ʱ��0��msg_type=1ʱΪ�豸���صĹ̼��汾˵����msg_type=3ʱ�豸���Ըò�����
	uint8_t  firmware[24];


	//�ı���ʽ�Ŀ��
	//msg_type=0ʱ��0��msg_type=1�豸���Ըò�����msg_type=3ʱ�豸�����ÿ������Ϸ��޸����������
	uint8_t  cfgpwd[12];


	//msg_type=0ʱ��0��msg_type=1�豸���ص�ǰ�ļ���IP��msg_type=3ʱ�豸���Ըò�����
	//ӳ��IP����mapport����ʹ�ã�mapip��mapport����ĵ�1��Ԫ��Ϊһ����ʹ�õ�TCP/IP���ӡ�
	//�磺mapip[0]��mapport[0]ָ���豸��һ��������IP��Port������ֱ��ʹ������������豸����TCP/IP���ӡ�
    uint32_t  mapip[32];


	//ӳ��˿�
	//msg_type=0ʱ��0��msg_type=1�豸���ص�ǰ�ļ���Port��msg_type=3ʱ�豸���Ըò�����
    uint32_t  mapport[32];         
} MUSTER_TELE;

typedef struct _MUSTER_TELE_V2 {
	//��ϢID��Ĭ��Ϊ4��0��
	uint32_t   xid;


	//��Ϣ���ͣ�0-��λ�������������ٻ��豸��1-�豸��Ӧ�ٻ����豸���أ���3-��λ�����豸������������
	uint8_t   msg_type;


	//muster�汾��2��
	uint8_t   muster_ver;


	//�Ҷ�������ݽṹ

} MUSTER_TELE_V2;


THREAD(udp_cmd_thread, arg)
{
	const uint8_t len = 7;
	uint8_t buff[len];

	uint32_t addr = 0;
	uint16_t port = 0;
	void      * socket = NULL;
	uint32_t  time = 1000;
	
	CmdIpConfigData ipconfig;
	NutThreadSetPriority(UDP_CLIENT_SERVER_PRI);

	BspReadIpConfig(&ipconfig);
	addr = group_arry4_to_uint32(ipconfig.ipaddr);
	port = ipconfig.port;
	if(THISINFO)printf("UDP Command process server start....\r\n");
		//UDPSOCKET * udp_socket = NULL;
	//TCPSOCKET * tcp_socket = NULL;
	while(1) 
	{
		uint8_t  count = 0;
		uint32_t host_ip = 0;
		uint16_t host_port = 0;
		host_address hostaddr;
		unsigned char host_type,ip_type;
		uint32_t last_input = GetFilterInput();
		BspReadHostAddress(&hostaddr);
		host_type = hostaddr.type >> 4;
		ip_type = hostaddr.type & 0x0F;
		//DNS����
		if(ip_type == HOST_ADDR_IS_IP_MODE) {
			host_ip  = (unsigned char)hostaddr.host_address[3],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[2],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[1],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[0];
			if(THISINFO)printf("ip type = 0 ,get host addr ip(%d.%d.%d.%d) is ok!\r\n",
					(uint8_t)((host_ip>>0)&0xFF),(uint8_t)((host_ip>>8)&0xFF),(uint8_t)((host_ip>>16)&0xFF),(uint8_t)((host_ip>>24)&0xFF));
		} else if(host_type != HOST_IS_TCP_CLIENT) { //���豸��Ϊ�������ǲ���Ҫ����������
			if(THISINFO)printf("ip type = 1 ,get host addr domain(%s)\r\n",(const char *)hostaddr.host_address);
		    if ((host_ip = NutDnsGetHostByName((u_char*)hostaddr.host_address)) != 0) {
				if(THISINFO)printf("get host addr ip(%d.%d.%d.%d) ok!\r\n",
					(uint8_t)((host_ip>>0)&0xFF),(uint8_t)((host_ip>>8)&0xFF),(uint8_t)((host_ip>>16)&0xFF),(uint8_t)((host_ip>>24)&0xFF));
			} else {
				if(THISINFO)printf("get host addr failed!wait some time to reget it.\r\n");
				NutSleep(5000);
				continue;
			}
		}
		host_port = hostaddr.port;
		if(host_port == ipconfig.port && host_type == HOST_IS_TCP_CLIENT) {
			if(THISINFO)printf("this port is equ ipconfig.port,this param failed!\r\n");
			NutSleep(5000);
			continue;
		}
		//����
		if(host_type == HOST_IS_UDP) {
			socket = NutUdpCreateSocket(host_port);
			if(socket != NULL) {
				if(THISINFO)printf("NutUdpCreateSocket(%d)\r\n",host_port);
				NutUdpSetSockOpt((UDPSOCKET *)socket, SO_RCVBUF, &len, sizeof(len));
			} else {
				if(THISERROR)printf("Create UDP Socket failed! try again...\r\n");
				NutSleep(5000);
				continue;
			}
		} else if(host_type == HOST_IS_TCP_SERVER || host_type == HOST_IS_TCP_CLIENT) {
			socket = NutTcpCreateSocket();
			if(socket != NULL) {
				time = 10000;
				NutTcpSetSockOpt((TCPSOCKET *)socket,SO_RCVTIMEO,&time,sizeof(uint32_t));
				NutTcpSetSockOpt((TCPSOCKET *)socket,SO_SNDTIMEO,&time,sizeof(uint32_t));
			} else {
				if(THISERROR)printf("Create TCP Socket failed! try again...\r\n");
				NutSleep(5000);
				continue;
			}
		} else {
			if(THISINFO)printf("get host type not valid!wait some time to reget it.\r\n");
			NutSleep(5000);
			continue;
		}
		//����
		if(host_type == HOST_IS_UDP)  while(1) {
			time = 50;
			int ret = NutUdpReceiveFrom((UDPSOCKET *)socket,&addr,&port,buff,len,time);
		    if(ret < 0) {
			    if(THISINFO)printf("UDP Receive error!\r\n");
		    } else if(ret == 0) {
			    //if(THISINFO)printf("UDP Receive 0\r\n");
			    //01 00 0F 00 00 00 00
				uint32_t input = GetFilterInput();
				count++;
				if(count > 60 || last_input != input) {
					unsigned char buffer[7];
					count = 0;
					if(last_input != input) {
						buffer[0] = 0x01;
					} else {
						buffer[0] = 0x00;
					}
					last_input = input;
					//
					buffer[1] = 0x00;
					buffer[2] = (uint8_t)((input>>0)&0xFF);
					buffer[3] = (uint8_t)((input>>8)&0xFF);
					buffer[4] = (uint8_t)((input>>16)&0xFF);
					buffer[5] = (uint8_t)((input>>24)&0xFF);
					buffer[6] = 0x00;
					//����
					// int NutUdpSendTo(UDPSOCKET *sock, uint32_t addr, uint16_t port, void *data, int len);
					if(!NutUdpSendTo((UDPSOCKET *)socket,host_ip,host_port,buffer,7)) {
						if(THISINFO)printf("UDP timeout update ok!   ");
					} else {
						if(THISINFO)printf("UDP timeout update failed!   ");
					}
				}
		    } else {
		        if(THISINFO)printf("UDP Receive %d bytes\r\n",ret);
			    //������
				if(ret >= 7) {
				    uint32_t out;
					if(THISINFO)printf("UDP Get One Tcp packet length(%d)\r\n",len);
					out  = (unsigned char)buff[5];out<<=8;
					out |= (unsigned char)buff[4];out<<=8;
					out |= (unsigned char)buff[3];out<<=8;
					out |= (unsigned char)buff[2];
					if(buff[0] == 0x00 || buff[0] == 0x01) {
						SetRelayWithDelay(out);
						if(THISINFO)printf("Set IO value = 0x%x\r\n",(unsigned int)(out&0xFF));
					} else if(buff[0] == 0x10) {
						RevertRelayWithDelay(out);
						if(THISINFO)printf("Sig IO value = 0x%x\r\n",(unsigned int)(out&0xFF));
					} else {
						if(THISINFO)printf("��������\r\n");
					}
				} else {
					if(THISERROR)printf("UDP Rx Data Len < 7 error!\r\n");
				}
		    }
		}
		if(host_type == HOST_IS_TCP_SERVER) {
			if(THISINFO)printf("wait TCP NutTcpConnect to server...\r\n");
			if(NutTcpConnect((TCPSOCKET *)socket,host_ip,host_port)) {
			    NutTcpCloseSocket((TCPSOCKET *)socket);
			    if(THISERROR)printf("NutTcpConnect failed! close socket and try again...\r\n");
				NutSleep(2000);
			    continue;
			}
			if(THISINFO)printf("Tcp2 NutTcpConnect successful.\r\n");
			goto tcp_process;
		} else if(host_type == HOST_IS_TCP_CLIENT) {
			//�����������ӽ���
			if(THISINFO)printf("wait TCP NutTcpAccept form client...\r\n");
		    if(NutTcpAccept((TCPSOCKET *)socket,host_port)) {
			    NutTcpCloseSocket((TCPSOCKET *)socket);
			    if(THISERROR)printf("NutTcpAccept failed! close it and try again...\r\n");
				NutSleep(2000);
			    continue;
		    }
			if(THISINFO)printf("Tcp2 Accept one connection.\r\n");
tcp_process:
			//���ͳ�ʱʱ��
			time = 50; //50ms��ɨ�������Ƶ��
			count = 0;
			NutTcpSetSockOpt((TCPSOCKET *)socket,SO_RCVTIMEO,&time,sizeof(uint32_t));
			time = 5000; //���ͳ�ʱ
			NutTcpSetSockOpt((TCPSOCKET *)socket,SO_SNDTIMEO,&time,sizeof(uint32_t));
			while(1) {
				int len = NutTcpReceive((TCPSOCKET *)socket,buff,sizeof(buff));
				if(len == 0) {
					//if(THISINFO)printf("Tcp2 Recieve timeout.\r\n");
					//NutTcpSend(sock,"OK",strlen("OK"));
					//�ϱ�����״̬
					count++;
					uint32_t input = GetFilterInput();
					if(count > 60 || last_input != input) {
					    unsigned char buffer[7];
					    count = 0;
					    if(last_input != input) {
						    buffer[0] = 0x01;
					    } else {
						    buffer[0] = 0x00;
					    }
					    last_input = input;
					    //
					    buffer[1] = 0x00;
					    buffer[2] = (uint8_t)((input>>0)&0xFF);
					    buffer[3] = (uint8_t)((input>>8)&0xFF);
					    buffer[4] = (uint8_t)((input>>16)&0xFF);
					    buffer[5] = (uint8_t)((input>>24)&0xFF);
					    buffer[6] = 0x00;
					    //TCP����
					    if(NutTcpSend((UDPSOCKET *)socket,buffer,7) == 7) {
						    if(THISINFO)printf("TCP timeout update send ok!   ");
					    } else {
					    	if(THISINFO)printf("TCP timeout update send failed!   ");
					    }
					}
				} else if(len == -1) {
					int error = NutTcpError((TCPSOCKET *)socket);
					if(THISERROR)printf("Tcp2 Receive ERROR(%d)\r\n",error);
					if(error == ENOTCONN)  {
						if(THISERROR)printf("Tcp2 Socket is not connected,break connecting\r\n");
						break;
					} else {
						if(THISERROR)printf("Tcp2 Socket is unknow error,break connecting\r\n");
						break;
					}
				} else if(len >= 7) {
					uint32_t out;
					if(THISINFO)printf("Tcp2 Get One Tcp packet length(%d)\r\n",len);
					out  = (unsigned char)buff[5];out<<=8;
					out |= (unsigned char)buff[4];out<<=8;
					out |= (unsigned char)buff[3];out<<=8;
					out |= (unsigned char)buff[2];
					if(buff[0] == 0x00 || buff[0] == 0x01) {
						SetRelayWithDelay(out);
						if(THISINFO)printf("Set IO value = 0x%x\r\n",(unsigned int)(out&0xFF));
					} else if(buff[0] == 0x10) {
						RevertRelayWithDelay(out);
						if(THISINFO)printf("Sig IO value = 0x%x\r\n",(unsigned int)(out&0xFF));
					} else {
						if(THISINFO)printf("��������\r\n");
					}
				} else {
					if(THISERROR)printf("Tcp2 Rx Data Len < 7 error!\r\n");
				}
			}
			if(THISINFO)printf("Tcp2 NutTcpCloseSocket.\r\n");
			NutTcpCloseSocket((TCPSOCKET *)socket);
			//NutSleep(2000);
		} //end tcp
	}
}

void StartUdpCmdServer(void)
{
	//NutThreadCreate("udp_auto_config_thread",  udp_auto_config_thread, 0, 1024);
	NutThreadCreate("udp_cmd_thread",          udp_cmd_thread,         0, 1024);
}

#endif
