
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


#define THISINFO       DEBUG_ON_INFO
#define THISERROR      DEBUG_ON_ERROR

#ifdef USE_AUTO_CONFIG

typedef struct _MUSTER_TELE {
	//消息ID，默认为4个0。
	uint32_t   xid;


	//消息类型，0-上位机发出，用于召唤设备，1-设备响应召唤（设备返回），3-上位机向设备发送配置数据
	uint8_t   msg_type;


	//muster版本，一直为1。
	uint8_t   muster_ver;


	//主机名，msg_type=0时置0，msg_type=1时为设备返回的主机名，msg_type=3时指定设备的新主机名。
	uint8_t   net_hostname[12];


	//网络物理地址，msg_type=0时置0，msg_type=1时为设备返回的MAC地址，
	//msg_type=3时，指定为要修改设备的Mac地址。
	uint8_t   net_mac[6];


	//IP地址，msg_type=0时置0，msg_type=1时为设备返回的IP地址，msg_type=3时设备修改IP为该地址。
	uint32_t  net_ip_addr;


	//子网络掩码，msg_type=0时置0，msg_type=1时为设备返回的掩码，msg_type=3时设备修改掩码为该掩码。
	uint32_t  net_ip_mask;


	//网关，保留参数，尚未使用。
	uint32_t  net_gateway;


	//产品型号,尚未使用。
	uint32_t  devmodel;


	//工作模式,0-无效，1-Server模式,2-Client模式,10-UDP模式
	//msg_type=0时置0，msg_type=1时为设备返回的工作模式，msg_type=3时设备忽略该参数。
	uint32_t  workmodel;


	//设备的端口数，msg_type=0时置0，msg_type=1时为设备返回的端口数量，msg_type=3时设备忽略该参数。
	uint32_t  portnum;


	//第一个网络监听，保留参数，尚未使用
	uint32_t  firstport;


	//文本形式的固件版本说明，
	//msg_type=0时置0，msg_type=1时为设备返回的固件版本说明，msg_type=3时设备忽略该参数。
	uint8_t  firmware[24];


	//文本形式的口令。
	//msg_type=0时置0，msg_type=1设备忽略该参数，msg_type=3时设备将检查该口令，如果合法修改自身参数。
	uint8_t  cfgpwd[12];


	//msg_type=0时置0，msg_type=1设备返回当前的监听IP，msg_type=3时设备忽略该参数。
	//映射IP，与mapport联合使用，mapip和mapport数组的第1个元素为一个可使用的TCP/IP连接。
	//如：mapip[0]与mapport[0]指出设备第一个监听的IP和Port。可以直接使用这个参数与设备建立TCP/IP连接。
    uint32_t  mapip[32];


	//映射端口
	//msg_type=0时置0，msg_type=1设备返回当前的监听Port，msg_type=3时设备忽略该参数。
    uint32_t  mapport[32];         
} MUSTER_TELE;

typedef struct _MUSTER_TELE_V2 {
	//消息ID，默认为4个0。
	uint32_t   xid;


	//消息类型，0-上位机发出，用于召唤设备，1-设备响应召唤（设备返回），3-上位机向设备发送配置数据
	uint8_t   msg_type;


	//muster版本，2。
	uint8_t   muster_ver;


	//我定义的数据结构

} MUSTER_TELE_V2;


static uint8_t buffer[sizeof(MUSTER_TELE)];


#if  0
THREAD(udp_auto_config_thread, arg)
{
	const uint16_t length = sizeof(buffer);
	uint32_t addr = 0;
	uint16_t port = 0;
	UDPSOCKET * socket = NULL;

    NutThreadSetPriority(AUTO_CONFIG_THREAD_PRI);
	if(THISINFO)printf("UDP Auto Config Server Started!\r\n");
	socket = NutUdpCreateSocket(UDP_AUTO_CONFIG_PORT);
	NutUdpSetSockOpt(socket, SO_RCVBUF, &length, sizeof(length));
    while(socket)
	{
		//NutUdpReceiveFrom(UDPSOCKET *sock, uint32_t *addr, uint16_t *port, void *data, int size, uint32_t timeout);
		int ret = NutUdpReceiveFrom(socket,&addr,&port,buffer,length,1000);
		if(ret < 0) {
			if(THISINFO)printf("UDP Receive error!\r\n");
		} else if(ret == 0) {
			//if(THISINFO)printf("UDP Receive 0\r\n");
		} else {
		    if(THISINFO)printf("UDP Receive %d bytes\r\n",ret);
			//解析包
			if(ret >= sizeof(MUSTER_TELE)) {
				MUSTER_TELE * recbuf = (MUSTER_TELE *)buffer;
				if(recbuf->msg_type == 0) {
					//配置结构体
					u_char use_mac[] = SYS_DEFAULT_MAC;
					CmdIpConfigData ipconfig;
					if(THISINFO)printf("Get Ip Config\r\n");
					memset(recbuf,0,sizeof(MUSTER_TELE));
					BspReadIpConfig(&ipconfig);
					recbuf->msg_type = 1;
					recbuf->muster_ver = 1;
					strcpy(recbuf->net_hostname,"EthRelay");
					memcpy(recbuf->net_mac,use_mac,6);
					recbuf->net_ip_addr  = ipconfig.ipaddr[3];recbuf->net_ip_addr<<=8;
					recbuf->net_ip_addr |= ipconfig.ipaddr[2];recbuf->net_ip_addr<<=8;
					recbuf->net_ip_addr |= ipconfig.ipaddr[1];recbuf->net_ip_addr<<=8;
					recbuf->net_ip_addr |= ipconfig.ipaddr[0];
					recbuf->net_ip_mask  = ipconfig.netmask[3];recbuf->net_ip_mask<<=8;
					recbuf->net_ip_mask |= ipconfig.netmask[2];recbuf->net_ip_mask<<=8;
					recbuf->net_ip_mask |= ipconfig.netmask[1];recbuf->net_ip_mask<<=8;
					recbuf->net_ip_mask |= ipconfig.netmask[0];
					recbuf->firstport = ipconfig.port;
					recbuf->workmodel = 2; //client模式
					recbuf->portnum = 1;
					strcpy(recbuf->firmware,"Ver1.0.0");
					strcpy(recbuf->cfgpwd,"12345678");
					recbuf->mapip[0]   = recbuf->net_ip_addr;
					recbuf->mapport[0] = recbuf->firstport;
					//NutUdpSendTo(UDPSOCKET *sock, uint32_t addr, uint16_t port, void *data, int len);
					NutUdpSendTo(socket,addr,port,recbuf,length);
				} else if(recbuf->msg_type == 3) {
					//配置
					CmdIpConfigData ipconfig;
					if(THISINFO)printf("Set Ip Config\r\n");
					ipconfig.ipaddr[0] = (uint8_t)(((recbuf->net_ip_addr>>0)&0xFF));
					ipconfig.ipaddr[1] = (uint8_t)(((recbuf->net_ip_addr>>8)&0xFF));
					ipconfig.ipaddr[2] = (uint8_t)(((recbuf->net_ip_addr>>16)&0xFF));
					ipconfig.ipaddr[3] = (uint8_t)(((recbuf->net_ip_addr>>24)&0xFF));
					ipconfig.netmask[0] = (uint8_t)(((recbuf->net_ip_mask>>0)&0xFF));
					ipconfig.netmask[1] = (uint8_t)(((recbuf->net_ip_mask>>8)&0xFF));
					ipconfig.netmask[2] = (uint8_t)(((recbuf->net_ip_mask>>16)&0xFF));
					ipconfig.netmask[3] = (uint8_t)(((recbuf->net_ip_mask>>24)&0xFF));
					ipconfig.gateway[0] = (uint8_t)(((recbuf->net_gateway>>0)&0xFF));
					ipconfig.gateway[1] = (uint8_t)(((recbuf->net_gateway>>8)&0xFF));
					ipconfig.gateway[2] = (uint8_t)(((recbuf->net_gateway>>16)&0xFF));
					ipconfig.gateway[3] = (uint8_t)(((recbuf->net_gateway>>24)&0xFF));
					ipconfig.dns[0] = ipconfig.gateway[0];
					ipconfig.dns[1] = ipconfig.gateway[1];
					ipconfig.dns[2] = ipconfig.gateway[2];
					ipconfig.dns[3] = ipconfig.gateway[3];
					ipconfig.port = (uint16_t)recbuf->firstport;
					ipconfig.webport = (uint16_t)80;
					//
					if(!BspWriteIpConfig(&ipconfig)) {
						//重启
						//RequestSystemReboot(); //已经调用了
					} else {
						if(!BspWriteIpConfig(&ipconfig)) {
							//重启
							//RequestSystemReboot();
						} else {
							//失败
						}
					}
				}
			}
		}
	}
	NutUdpDestroySocket(socket);
	return 0;
}

#endif

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
		//DNS解析
		if(ip_type == HOST_ADDR_IS_IP_MODE) {
			host_ip  = (unsigned char)hostaddr.host_address[3],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[2],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[1],host_ip <<= 8;
			host_ip |= (unsigned char)hostaddr.host_address[0];
			if(THISINFO)printf("ip type = 0 ,get host addr ip(%d.%d.%d.%d) is ok!\r\n",
					(uint8_t)((host_ip>>0)&0xFF),(uint8_t)((host_ip>>8)&0xFF),(uint8_t)((host_ip>>16)&0xFF),(uint8_t)((host_ip>>24)&0xFF));
		} else if(host_type != HOST_IS_TCP_CLIENT) { //本设备作为服务器是不需要域名解析的
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
		//创建
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
		//服务
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
					//发送
					// int NutUdpSendTo(UDPSOCKET *sock, uint32_t addr, uint16_t port, void *data, int len);
					if(!NutUdpSendTo((UDPSOCKET *)socket,host_ip,host_port,buffer,7)) {
						if(THISINFO)printf("UDP timeout update ok!   ");
					} else {
						if(THISINFO)printf("UDP timeout update failed!   ");
					}
				}
		    } else {
		        if(THISINFO)printf("UDP Receive %d bytes\r\n",ret);
			    //解析包
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
						if(THISINFO)printf("其他命令\r\n");
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
			//接受主机连接进来
			if(THISINFO)printf("wait TCP NutTcpAccept form client...\r\n");
		    if(NutTcpAccept((TCPSOCKET *)socket,host_port)) {
			    NutTcpCloseSocket((TCPSOCKET *)socket);
			    if(THISERROR)printf("NutTcpAccept failed! close it and try again...\r\n");
				NutSleep(2000);
			    continue;
		    }
			if(THISINFO)printf("Tcp2 Accept one connection.\r\n");
tcp_process:
			//降低超时时间
			time = 50; //50ms，扫描输入口频率
			count = 0;
			NutTcpSetSockOpt((TCPSOCKET *)socket,SO_RCVTIMEO,&time,sizeof(uint32_t));
			time = 5000; //发送超时
			NutTcpSetSockOpt((TCPSOCKET *)socket,SO_SNDTIMEO,&time,sizeof(uint32_t));
			while(1) {
				int len = NutTcpReceive((TCPSOCKET *)socket,buff,sizeof(buff));
				if(len == 0) {
					//if(THISINFO)printf("Tcp2 Recieve timeout.\r\n");
					//NutTcpSend(sock,"OK",strlen("OK"));
					//上报输入状态
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
					    //TCP发送
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
						if(THISINFO)printf("其他命令\r\n");
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
