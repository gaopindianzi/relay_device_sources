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
	unsigned char device_time[6];
	unsigned char work_port[2];   //本地的UDP通信端口号
	unsigned char remote_host_addr[32]; //远程主机的IP地址
	unsigned char remote_host_port[2]; //远程主机UDP端口号
	//主板型号
	unsigned char device_model;
	//选项，是否关闭其他功能
	unsigned char system_fun_option;
	//更新周期
	unsigned char broadcast_time;  //广播时间，单位s,0不广播,1以上广播
	//本地信息
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
	unsigned char command_len;
	unsigned char modbus_buffer[1];  //指向内存
} modbus_command_st;


#endif


