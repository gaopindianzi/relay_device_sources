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
	char          host_name[64];
	unsigned char mac[6];
	char          group_name1[32];
	char          group_name2[32];
    char          password[20];   //ͨ���õĵ�ַ
	unsigned char device_time[6];
	unsigned char work_port[2];   //���ص�UDPͨ�Ŷ˿ں�
	unsigned char remote_host_addr[32]; //Զ��������IP��ַ
	unsigned char remote_host_port[2]; //Զ������UDP�˿ں�
	//�����ͺ�
	unsigned char device_model;
	//ѡ��Ƿ�ر���������
	unsigned char system_fun_option;
	//��������
	unsigned char broadcast_time;  //�㲥ʱ�䣬��λs,0���㲥,1���Ϲ㲥
	//������Ϣ
	unsigned char local_ip[4];
	unsigned char net_mask[4];
	unsigned char gateway[4];
	unsigned char dns[4];
	//�����ϵ����ݽ���CRCУ��
	unsigned char crc[2];
} device_info_st;


typedef struct __modbus_command_st
{
	unsigned char command;
	unsigned char command_len;
	unsigned char modbus_buffer[1];  //ָ���ڴ�
} modbus_command_st;


#endif

