
/*
* 2012-5-7，为了实现多管理，增加下位机接口，实现以下目标：
  //首先是配置
  配置设备的使用密码,IP地址，UDP端口号，内部IP地址，子网掩码，网关、DNS主机地址,一级组，二级组ID号，


  //然后是使用
  局域网内，设备自动广播，通过获取广播消息，上位机自动添加主机信息，主机信息有：主机名字，主机唯一ID号或MAC码，一级组，二级组ID号，设备管理IP地址和UDP端口号
  广域网外，设备或自动连接用户设定的一个主机地址，通过UDP发送出去，远程主机通过检测接收到的UDP包，解析上诉的主机信息
  上位机通过动态或人工添加的主机信息列表，可以导出到一个文件中

  //改为对称加密，双方约定一个秘密，一旦密码泄露，设备需要修改密码，不过这个可以轻松实现，通信的内容都经过加密
  //在设置模式下，无需加密，直接传输原文
  //在工作模式下，必须加密，原文无法接受
  //指令定义：
  //主机信息获取和配置命令，MODBUS扩展控制命令，其他扩展命令，共两条新命令，在一个UDP接口发送和接收
  //如果设备在1分钟内没有接收到控制命令，则会20秒钟广播自己的主机信息，然后往指定IP主机发送自己的主机信息，但不包括密码。
  //主机在规定时间内，于设备通信失败，则显示离线，然后显示重新连接主机中。
  //界面
  //分两个界面，一个是登陆设置界面，另外一个列表显示界面，
  //在线的主机，都可以单选此主机，和全算此主机，然后右侧会显示控制，控制开或关的按钮
  //如果多选，则右侧显示最多路数的主机控制按钮个数，但不现实主机信息
  //设备列表可以按照主机名字排列，按照ID1排列，ID2排列,ID3排列，以方便管理和分组排列
  //所有数据都经过CRC校验
  //通信过程中的一般都有返回数据包，如果设备检测到密码错，则返回一个包，说明密码错误，如果指令不支持，则说明指令不支持，如果其他错误则返回未知错误。
  //
  //
  //DLL实现方式，没有事件触发模式。只能查询方式，用途广泛，但功能软弱
  //数据库做桥梁，后台和前台的模式，前后台通信成本高，不适用。
  //分模块，定义系统接口实现方式，接口属于软接口模式，扩展性不好暂时采纳。
  //
*/


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

#include "bin_command_def.h"
#include "bsp.h"
#include "sysdef.h"
#include "cgi_thread.h"
#include "server.h"
#include "time_handle.h"
#include "can_485_uart.h"
#include "udp_client_command.h"
#include "modbus_interface.h"
#include <dev/reset_avr.h>
#include <dev/usartavr485.h>
#include <cfg/platform_def.h>
#include "multimgr_device.h"

#include "bsp.h"

#define THISINFO       0
#define THISERROR      0


//#define DEFINE_TEST_WDT_RESET   //是否用看门狗测试一下复位情况

#define NICINB(reg)         (*((volatile uint8_t *)0x8300 + reg))


#if  0 //测试宏定义
#if BOARD_TYPE   ==  EXT_BOARD_IS_2CHIN_2CHOUT_BOX
#error  "EXT_BOARD_IS_2CHIN_2CHOUT_BOX test successful"
#endif

#if BOARD_TYPE   ==  EXT_BOARD_IS_4CHIN_4CHOUT
#error  "EXT_BOARD_IS_4CHIN_4CHOUT test successful"
#endif

#if BOARD_TYPE   ==  EXT_BOARD_IS_8CHIN_8CHOUT_V2
#error  "EXT_BOARD_IS_8CHIN_8CHOUT_V2 test successful"
#endif

#if BOARD_TYPE   ==  EXT_BOARD_IS_16CHOUT
#error  "EXT_BOARD_IS_16CHOUT test successful"
#endif


#endif


#ifdef SYS_DEFAULT_MAC
//#error SYS_DEFAULT_MAC
#endif



FILE * resetfile = NULL;

//多管理设备信息
extern device_info_st    multimgr_info;
//


uint32_t toheip(u8_t ip[]);
int BspReadIpConfig(CmdIpConfigData * cid);
void io_time_tick_add_server(uint16_t tick_ms);
extern const unsigned char app_check_data[256];
extern volatile unsigned char addr_low;
extern volatile unsigned char error_flag;
extern void Load_check_dada(void);
extern void StartModbus_Interface(void);

uint32_t  ipconfig_dns;
extern char gpassword[32];

int main(void)
{
    u_char de_mac[] = SYS_DEFAULT_MAC;
	
    u_long baud = 115200;
	uint16_t count = 0;
	uint8_t  config;
	uint8_t  reset_type;
	u_long   millis;
	u_long   wait;
	int      direction;

	//addr_low = (unsigned char)(((unsigned int)app_check_data)&0xFF);
	//error_flag = 0x00;
	IoPortInit();
	BspDebugLedSet(0x3);
	NutSleep(3000);
	BspDebugLedSet(0x0);
	//NutSleep(1000);

	//注册调试接口
	NutRegisterDevice(&DEV_DEBUG, 0, 0);
    freopen(DEV_DEBUG_NAME, "w", stdout);
    _ioctl(_fileno(stdout), UART_SETSPEED, &baud);


	//Load_check_dada();

	NutThreadSetPriority(MAIN_RTC_TIME_PRI);

	if(THISINFO)printf("\r\n\r\nOS Start %s \r\n",NutVersionString());


	if(BspReadFactoryOut() != 0x55) {
		CmdIpConfigData cid;
		device_info_st  info;
		//初始化出厂数据
		if(THISINFO)printf("Initialize factory data......\r\n");
		//初始化IP地址
		cid.ipaddr[0] = 192; cid.ipaddr[1] = 168; cid.ipaddr[2] = 1; cid.ipaddr[3] = 249;
		cid.netmask[0] = 255; cid.netmask[1] = 255; cid.netmask[2] = 255; cid.netmask[3] = 0;
		cid.gateway[0] = 192; cid.gateway[1] = 168; cid.gateway[2] = 1; cid.gateway[3] = 1;
		cid.dns[0] = 192; cid.dns[1] = 168; cid.dns[2] = 1; cid.dns[3] = 1;
		cid.port = 2000;
		cid.webport = 80;
		BspWriteIpConfig(&cid);
		BspWriteIpConfig(&cid);
		//初始化端口号
		strcpy(info.password,"admin");
		info.cncryption_mode = 1;
		strcpy(info.host_name,"shen zhen jingruida network");
		strcpy(info.group_name1,"group1");
		strcpy(info.group_name2,"group2");
		strcpy(info.remote_host_addr,"szmcu.meibu.com");
		info.work_port[0] = (unsigned char)(505&0xFF);
		info.work_port[1] = (unsigned char)(505>>8);
		info.remote_host_port[0] = (unsigned char)(505&0xFF);
		info.remote_host_port[1] = (unsigned char)(505>>8);
		info.broadcast_time = BROADCASTTIME_MIN;
		BspSavemultimgr_info(&info);
		BspSavemultimgr_info(&info);
		//WEB密码
		strcpy(gpassword,"admin");
		BspWriteWebPassword(gpassword);
		BspWriteWebPassword(gpassword);
		//保存更新
		BspWriteFactoryOut(0x55);
		BspWriteFactoryOut(0x55);
	} else {
		if(THISINFO)printf("not init factory data.\r\n");
	}


	if(0 && THISINFO) {
		NutRegisterDevice(&devUart4851, 0, 0);
		FILE *file485 = fopen("uart4850", "w+b");
		if(!file485) {
			printf("file485 1  no valid.\r\n");
		} else {
			printf("file485 1  valid.\r\n");
		}
		fclose(file485);

		file485 = fopen("uart4851", "w+b");
		if(!file485) {
			printf("file485 2 no valid.\r\n");
		} else {
			printf("file485 2 valid.\r\n");
		}
		fclose(file485);

	}


	//注册复位，看门狗控制器
	NutRegisterDevice(&devAvrResetCtl, 0, 0);
	//注册输入输出接口驱动
	NutRegisterDevice(&devRelayInputOutput, 0, 0);


	
	count = 0x1234;
	config = ((unsigned char *)&count)[0];

	if(THISINFO)printf("Ending config = 0x1234 little = 0x%x\r\n",config);
	count = 0;


#ifdef APP_TIMEIMG_ON
	if(THISINFO)printf("sizeof(timing_node_eeprom)=%d,BSP_MAX_OFFSET=0x%x\r\n",sizeof(timing_node_eeprom),BSP_MAX_OFFSET);
#endif


	if(THISINFO) {
		unsigned int reg;
		reg   = NICINB(0x0A);
		reg <<= 8;
        reg  += NICINB(0x0B);
		if(THISINFO)printf("read rtl8019 chip reg = 0x%x\r\n",reg);
		if(THISINFO)printf("start check external static ram...\r\n");
		if(1) {
			unsigned char * pbuf = NULL;
			unsigned int len = 1;
			unsigned int i;
			while(1) {
				pbuf = malloc(len);
				if(pbuf) {
					free(pbuf);
					pbuf = NULL;
				} else {
					len -= 1;
					pbuf = malloc(len);
					break;
				}
				len += 1;
			}
			printf("extern malloc max = %d bytes \r\n",len);
			for(i=0;i<len;i++) {
				volatile unsigned char reg;
				pbuf[i] = 0xAA;
				reg = 0;
				reg = pbuf[i];
				if(reg != 0xAA) {
					break;
				} else {
					if(THISINFO && i % 1000 == 0)printf("%d,",i);
				}
			}
			if(i != len) {
				if(THISINFO)printf("\r\ntest external static ram fialed,at %d!\r\n",i);
			} else {
				if(THISINFO)printf("\r\ntest external static ram successful!\r\n");
			}
			free(pbuf);
		}
	}


	{ //获取复位信息
	    resetfile = fopen("resetctl", "w+b");
		_ioctl(_fileno(resetfile), GET_RESET_TYPE, &reset_type);
		//fclose(resetfile);
	}


	if(0) {  //for test i2c interface.
		struct _tm  sys_time;
		sys_time.tm_sec = 1;
		sys_time.tm_min = 5;
		sys_time.tm_hour = 5;
		sys_time.tm_mday = 5;
		sys_time.tm_mon = 5;
		sys_time.tm_year = 55;
		sys_time_ms = 0;

		    if(THISINFO)printf("Registering RTC hardware...");
    if (NutRegisterRtc(&RTC_CHIP)) {
        if(THISERROR)puts("NutRegisterRtc failed");
    } else {
		if(THISINFO)puts("NutRegisterRtc OK");
    }
	while(1) {



		if(!NutRtcSetTime(&sys_time)) {
			if(THISERROR)printf("set rtc successful!\r\n");
		} else {
			if(THISERROR)printf("set rtc fialed!\r\n");
		}
	}
	}



	if(!sys_time_init()) {
		if(THISINFO)puts("sys_time_init OK");
	} else {
		if(THISINFO)puts("sys_time_init failed");
	}
	//
	if(reset_type&AVR_EXT_RESET) {
		//复位
		if(THISINFO)printf("Ext reset init io out.\r\n");
		BspIoInInit();
		BspIoOutInit();
#ifdef  ONE_273_INPUT_MODE
	    bsp_input_output_init();
#endif
	} else {
	    if(THISINFO)printf("Other reset,no init.\r\n");
	    if(reset_type&AVR_JTAG_RESET) {
		  if(THISINFO)puts("AVR_JTAG_RESET\r\n");
		  //BspIoOutInit();
	    }
	    if(reset_type&AVR_WDT_RESET) {
		  if(THISINFO)puts("AVR_WDT_RESET\r\n");
	    }
	    if(reset_type&AVR_POWER_DOWN_RESET) {
		  if(THISINFO)puts("AVR_POWER_DOWN_RESET\r\n");
		  //BspIoOutInit();
	    }
	    if(reset_type&AVR_POWER_UP_RESET) {
		  if(THISINFO)puts("AVR_POWER_UP_RESET\r\n");
		  //BspIoOutInit();
	    }
	}


	//移到这里是否有问题？未测试
	BspManualCtlModeInit();
	StartIoOutControlSrever();
	//??

	while(0) {
		NutSleep(100);
	    BspDebugLedSet(0x3);
	    NutSleep(100);
	    BspDebugLedSet(0x0);
	}


    if (NutRegisterDevice(&DEV_ETHER, 0, 0)) {
        if(THISINFO)puts("Registering device failed");
	} else {
		if(THISINFO)puts("Registering ethernet device ok.");
	}
	
	config = IoGetConfig();
	if(THISINFO)printf("Get Config param(0x%x)\r\n",config);

	if(THISINFO)printf("Configure %s...\r\n", DEV_ETHER_NAME);
	
	NutNetLoadConfig(DEV_ETHER_NAME);
	






	//config = 0x3;

	if(config & (1<<0)) {
		u_long ip_addr;
		u_long ip_mask;
		u_long ip_gate;
		u_long ip_dns;
		gwork_port = 2000;
		gweb_port = 80;
		if(config&(1<<1)) {
			if(THISINFO)printf("config ip address:192.168.1.250\r\n");
			ip_addr = inet_addr("192.168.1.250");
			ip_mask = inet_addr("255.255.255.0");
			ip_gate = inet_addr("192.168.1.1");
			ip_dns  = inet_addr("192.168.1.1");
		} else {
			if(THISINFO)printf("config ip address:192.168.0.250\r\n");
			ip_addr = inet_addr("192.168.0.250");
			ip_mask = inet_addr("255.255.255.0");
			ip_gate = inet_addr("192.168.0.1");
			ip_dns  = inet_addr("192.168.0.1");
		}

		confnet.cdn_cip_addr = ip_addr;
		confnet.cdn_ip_mask = ip_mask;
		confnet.cdn_gateway = ip_gate;
		ipconfig_dns = ip_dns;

        if (NutNetIfConfig(DEV_ETHER_NAME, de_mac, ip_addr, ip_mask) == 0) {
            /* Without DHCP we had to set the default gateway manually.*/
            if(THISINFO)puts("OK");
        }
        else {
            if(THISINFO)puts("failed");
        }
		NutIpRouteAdd(0, 0, ip_gate, &DEV_ETHER);
		NutDnsConfig2(0,0,ip_dns,(uint32_t)inet_addr("202.96.134.133"));
		goto config_finish;
	} else {
		if(config&(1<<1)) { //使用内部地址
			CmdIpConfigData cid;
		    u_long ip_addr;
		    u_long ip_mask;
		    u_long ip_gate;
			u_long ip_dns;
			if(BspReadIpConfig(&cid)) {
				if(THISINFO)printf("no default ip addr\r\n");
				goto no_default;
			}
			gwork_port = cid.port;
			gweb_port  = cid.webport;
			ip_addr = toheip(cid.ipaddr);
			ip_mask = toheip(cid.netmask);
			ip_gate = toheip(cid.gateway);
			ip_dns  = toheip(cid.dns);

			confnet.cdn_cip_addr = ip_addr;
			confnet.cdn_ip_mask = ip_mask;
			confnet.cdn_gateway = ip_gate;
			ipconfig_dns = ip_dns;

			if(THISINFO)printf("config ip address:     %s\r\n",inet_ntoa(ip_addr));
			if(THISINFO)printf("config netmask address:%s\r\n",inet_ntoa(ip_mask));
			if(THISINFO)printf("config gateway address:%s\r\n",inet_ntoa(ip_gate));
			if(THISINFO)printf("config dns address:    %s\r\n",inet_ntoa(ip_dns));
			//
            if (NutNetIfConfig(DEV_ETHER_NAME, de_mac, ip_addr, ip_mask) == 0) {
                /* Without DHCP we had to set the default gateway manually.*/
                if(THISINFO)puts("OK");
            }
            else {
                if(THISINFO)puts("failed");
            }
			NutIpRouteAdd(0, 0, ip_gate, &DEV_ETHER);
			NutDnsConfig2(0,0,ip_dns,ip_dns);
			goto config_finish;
		} else {
no_default:
		//没有配置
		//自动分配
			gwork_port = 2000;
			ipconfig_dns = 0;
			if(THISINFO)printf("config ip with DHCP server.\r\n");
			if (!NutDhcpIfConfig(DEV_ETHER_NAME, de_mac, 60000)) {
				if(THISINFO)printf("DHCP config success,%s ready!\r\n",inet_ntoa(confnet.cdn_ip_addr));
			} else {
				//分配失败。没有网络功能
				if(THISINFO)printf("DHCP config failed!\r\n");
				_ioctl(_fileno(resetfile), SET_RESET, NULL);
				for(;;);
			}
			goto config_finish;
		}
	}
config_finish:
    
	NutRegisterDiscovery((u_long)-1, 0, DISF_INITAL_ANN);
	//
	//
#ifdef APP_CGI_ON
	StartCGIServer();
	NutRegisterDevice(&MY_FSDEV, 0, 0);
#endif
	StartBinCmdServer();
#ifdef USE_AUTO_CONFIG
	StartUdpCmdServer();
#endif

#ifdef APP_485_ON
	StartCAN_485Srever();
#endif


#ifdef APP_MULTI_MANGER_FRAME
	StratMultiMgrDeviceThread();
#endif



#ifdef APP_MODBUS_TCP_ON
	StartModbus_Interface();
#endif

	while(0) {
		NutSleep(1000);
		printf("..\r\n");
	}


	if(0 && THISINFO) {
		unsigned char input;
		unsigned int count = 0;
		printf("Start io test...\r\n");
		while(1) {
			//input = clock_out_clock_in2(0x0F);
			BspDebugLedSet(0x0);
			printf("%d:input=0x%x\r\n",count++,input);
			NutSleep(500);
			//input = clock_out_clock_in2(0x00);
			BspDebugLedSet(0x3);
			printf("%d:input=0x%x\r\n",count++,input);
			NutSleep(500);
			if(count > 10) break;
		}
	} else {
		//BspDebugLedSet(0x0);
		//NutSleep(1000);
	}



	//InitSysWdt();
	count = 0;
	direction = 0;
	if(THISINFO)printf("-------------------------main loop start--------------------------\r\n");
	millis = NutGetMillis();
	baud = 0;
    while(1) {
		const unsigned char sys_tick_time_ms = 10;

#define  VIEW_TIME_TICK         0  //  THISINFO

		//if(THISINFO)printf("wdt");
		//ResetSystemWdt();
		if(count >= 100) {
			if(THISINFO) {
			    now_time now = get_system_time();
			    tm rtctime = get_system_rtc_time();
			    if(VIEW_TIME_TICK)printf("SYSTEM:%d-%d-%d  %d-%d-%d     DS1307:%d-%d-%d  %d-%d-%d\r\n",
				    now.sys_time.tm_year+1900,now.sys_time.tm_mon+1,now.sys_time.tm_mday,now.sys_time.tm_hour,now.sys_time.tm_min,now.sys_time.tm_sec,
				    rtctime.tm_year+1900,rtctime.tm_mon+1,rtctime.tm_mday,rtctime.tm_hour,rtctime.tm_min,rtctime.tm_sec);
			}
			count = 0;
		}
		{
		    if(direction < 0) {
				direction = time_add_millisecond(sys_tick_time_ms*2);
			    if(VIEW_TIME_TICK)printf("fast it\r\n");
		    } else if(direction > 0) {
				direction = time_add_millisecond(1);
			    if(VIEW_TIME_TICK)printf("slow it\r\n");
		    } else {
				direction = time_add_millisecond(sys_tick_time_ms);
		    }
		}
		wait = millis + sys_tick_time_ms - NutGetMillis();
		if(wait > 0 && wait < sys_tick_time_ms*2) {
			NutSleep(wait);
		}
		count++;
		millis += sys_tick_time_ms;
#ifdef DEFINE_TEST_WDT_RESET
		if(++baud == 30*100) {
			//启动
			if(1)printf("reset system ...\r\n");
			_ioctl(_fileno(resetfile), SET_RESET, NULL);
		}
#endif
	}
    return 0;
}



