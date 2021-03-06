#ifndef __MULTIMGR_DEVICE_DEV_H__
#define __MULTIMGR_DEVICE_DEV_H__

#define CMD_GET_DEVICE_INFO     0
#define CMD_SET_DEVICE_INFO     1
#define CMD_MODBUSPACK_SEND     2
#define CMD_RESET_DEVICE        3



typedef struct __device_info_st
{
    unsigned char command;
    unsigned char command_len;
    char          to_host;
    char          host_name[64];
    unsigned char mac[6];
    char          group_name1[32];
    char          group_name2[32];
    char          change_password;
    char          password[20];   //通信用的地址
    char          cncryption_mode;
    unsigned char device_time[6];
    unsigned char work_port[2];   //本地的UDP通信端口号
    char          remote_host_addr[64]; //远程主机的IP地址
    unsigned char remote_host_port[2]; //远程主机UDP端口号
    //主板型号
    unsigned char device_model;
    //选项，是否关闭其他功能
    unsigned char system_fun_option;
    //更新周期
    unsigned char broadcast_time;  //广播时间，单位s,0不广播,1以上广播
    //本地信息
    char          change_ipconfig;
    unsigned char local_ip[4];
    unsigned char net_mask[4];
    unsigned char gateway[4];
    unsigned char dns[4];
    //对以上的数据进行CRC校验
    unsigned char crc[2];
} device_info_st;



typedef struct __modbus_command_st
{
	unsigned char command;
    unsigned char crc[2]; //对以下内容进行CRC校验
	unsigned char command_len;
} modbus_command_st;

#define  GET_MODBUS_COMMAND_DATA(ptr)   ((void *)(((unsigned char *)(ptr))+sizeof(modbus_command_st)))


typedef struct __reset_device_st
{
	unsigned char command;
	unsigned char command_len;
	unsigned char pad[64];
	unsigned char crc[2];
} reset_device_st;










#ifdef APP_HTTP_PROTOTOL_CLIENT

//客户机版本的数据结构
typedef struct _ethernet_relay_info
{
	unsigned char factory_mode;
	char id[15];
	volatile unsigned char enable;
	char host_addr[128];
	char web_page[64];
	unsigned int port;
	unsigned int up_time_interval;
} ethernet_relay_info;

extern ethernet_relay_info sys_info;
extern void StartHttpRequestThread(void);

#endif










#endif






