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
#include <time.h>
#include <sys/mutex.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "sysdef.h"
#include "bin_command_def.h"
#include "bsp.h"
#include "time_handle.h"
#include "io_time_ctl.h"
#include <dev/relaycontrol.h>
#include "sys_var.h"
#include "io_out.h"
#include "debug.h"


#define THISINFO          0
#define THISERROR         0
#define THISASSERT        1


#define  IO_OUT_COUNT_MAX            32
#define  IO_IN_COUNT_MAX             32
#define  INPUT_HOLD_COUNT            5  //

//
const unsigned char  code_msk[8] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

unsigned char last_input[BITS_TO_BS(IO_IN_COUNT_MAX)]     __attribute__ ((section (".noinit")));
unsigned char input_flag[IO_IN_COUNT_MAX/8]     __attribute__ ((section (".noinit")));
unsigned char input_filter[IO_IN_COUNT_MAX/8]   __attribute__ ((section (".noinit")));
unsigned char input_hold[IO_IN_COUNT_MAX]       __attribute__ ((section (".noinit")));
unsigned char switch_input_control_mode[IO_IN_COUNT_MAX];


//------------------------------------------------------------------------------------------------
//�����޸����봥���Ĵ���
//���봥�����������룺��ʱ����ʱ�䣬������ʱʱ��
//���봥��ģʽÿ�ֶ�������ʱ��������
//Ĭ���ϵ���ǲ������ġ�ֻ�����е�ʱ�����˸ı����Ҫ��������˼��˵�ϵ���ʼ������Ҫ���������״̬����ʼ��
//unsigned char input_trigger_signals[BITS_TO_BS(INPUT_CHANNEL_NUM)]; //�������ģʽ�ı�־
unsigned char input_current_flag[BITS_TO_BS(INPUT_CHANNEL_NUM)];
unsigned char input_trigger_state[INPUT_CHANNEL_NUM]; //����״̬
unsigned int  input_filter_hold_time[INPUT_CHANNEL_NUM],input_filter_hold_time_max[INPUT_CHANNEL_NUM];
unsigned int  input_trig_before_delay[INPUT_CHANNEL_NUM];
unsigned int  input_trig_before_delay_max[INPUT_CHANNEL_NUM],input_trig_after_delay_max[INPUT_CHANNEL_NUM];
unsigned char input_trig_mode[INPUT_CHANNEL_NUM];
unsigned char input_trig_to_witch_io_out[INPUT_CHANNEL_NUM]; //��Ҫ������һ·��ֻ��ӳ��һ·

#define TRIGGER_NONE       0
#define TRIGGER_BEFORE     1
#define TRIGGER_HOLD       2
#define TRIGGER_AFTER      3





//�ϵ��ʼ����������
//�ⲿ�������ǣ�����ȼ��̳����Ѿ�׼������
void input_power_on_init(void)
{
	unsigned int  num = io_in_get_bits(0,input_current_flag,INPUT_CHANNEL_NUM);
	ASSERT(num==INPUT_CHANNEL_NUM);
	unsigned int i;
	for(i=0;i<INPUT_CHANNEL_NUM;i++) {
		input_trigger_state[i] = TRIGGER_NONE;
		input_filter_hold_time[i] = 0;
		input_filter_hold_time_max[i] = 0;
		input_trig_before_delay[i] = 0;
		input_trig_after_delay_max[i] = 0;
		input_trig_to_witch_io_out[i] = i;
	}
}

//ʵʱ�ظ��¼��̼�ֵ���˲����룬Ȼ����������ź�
void get_filter_input_flag(void)
{
	unsigned int i;
	unsigned char input_temp_flag[BITS_TO_BS(INPUT_CHANNEL_NUM)];
	unsigned int  num = io_in_get_bits(0,input_temp_flag,INPUT_CHANNEL_NUM);
	ASSERT(num==INPUT_CHANNEL_NUM);
	for(i=0;i<INPUT_CHANNEL_NUM;i++) {
		unsigned char reg0x1 = 0x01;
		unsigned char reg0x0 = 0x00;
		unsigned char p,b;
		p = i / 8;
		b = i % 8;
		if(input_temp_flag[p]&code_msk[b]) {
			if(input_current_flag[p]&code_msk[b]) {
				//����
				//�����ƽ���ƿ�ģʽ
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_ON_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
				//�����ƽ���ƹ�ģʽ
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_OFF_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
			} else {
				//������
				if(input_filter_hold_time[i] < input_filter_hold_time_max[i]) {
					input_filter_hold_time[i]++;
				} else {
				    input_current_flag[p] |=  code_msk[b];
				    //����
				    //������
				    if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_AFTER)) {
					    //������ģʽ��ǰ��ʱ����
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}
					//������ת��ʱ��
					if(input_trig_mode[i] == INPUT_TRIGGER_FLIP_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}
					//������ͨģʽ
					if(input_trig_mode[i] == INPUT_TRIGGER_TO_OPEN_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
					//�����ر�ģʽ
					if(input_trig_mode[i] == INPUT_TRIGGER_TO_OFF_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
					//���ش���ģʽ
					if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_AFTER)) { 
							//���ش�����ֻ�н��뵽����״̬�����ܽ��������ش���
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}

				}
			}
		} else {
			if(input_current_flag[p]&code_msk[b]) {
				//�½���
				if(input_filter_hold_time[i] > 0) {
					input_filter_hold_time[i] -= 10;
				} else {
					input_current_flag[p] &= ~code_msk[b];
					//����
					//���ش���ģʽ
					if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && 
						(input_trigger_state[i] == TRIGGER_BEFORE || input_trigger_state[i] == TRIGGER_HOLD)) { 
						//���ش�����ֻ�н��뵽����״̬�����ܽ����½��ش���
						input_trig_before_delay[i] = input_trig_after_delay_max[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
				}
			} else {
				//����
				input_filter_hold_time[i] = 0;
				//����
				//�����ƽ���ƿ�ģʽ
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_ON_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
				//�����ƽ���ƹ�ģʽ
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_OFF_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
			}
		}
	}
}

#if 0
#define INPUT_TRIGGER_FLIP_MODE          0x00  //������תģʽ
#define INPUT_SINGLE_TRIGGER_MODE        0x01  //������ģʽ
#define INPUT_TRIGGER_TO_OPEN_MODE       0x02  //������ͨģʽ
#define INPUT_TRIGGER_TO_OFF_MODE        0x03  //�����ر�ģʽ
#define INPUT_EDGE_TRIG_MODE             0x04  //���ش���ģʽ
#define INPUT_LEVEL_CTL_ON_MODE          0x05  //�����ƽ���ƿ�ģʽ
#define INPUT_LEVEL_CTL_OFF_MODE         0x06  //�����ƽ���ƹ�ģʽ
#define INPUT_TRIGGER_OFF_MODE           0x07  //���ƹر�
/*
*/
#endif

//������ʱ����
void trigger_timeout_handle(void)
{
	unsigned char reg0x1 = 0x01;
	unsigned char reg0x0 = 0x00;
	unsigned int i;
	for(i=0;i<INPUT_CHANNEL_NUM;i++) {
		//ʲô״̬��ʲôģʽ��ִ��ʲô����
		if(input_trig_before_delay[i] == 0) {
			//������
		    if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//������ģʽ��ǰ��ʱ����
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trig_before_delay[i] = input_trig_after_delay_max[i];
				input_trigger_state[i] = TRIGGER_HOLD;
		    } else if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && input_trigger_state[i] == TRIGGER_HOLD) {
				//������ģʽ��ǰ��ʱ����
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_AFTER;
			}
			//������ת��ʱ��
			if(input_trig_mode[i] == INPUT_TRIGGER_FLIP_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//��ת
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//�Ժ�ʹ��ڱ���״̬
			}
			//������ͨģʽ
			if(input_trig_mode[i] == INPUT_TRIGGER_TO_OPEN_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//��ת
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_HOLD;//�Ժ�ʹ��ڱ���״̬
			}
			//�����ر�ģʽ
			if(input_trig_mode[i] == INPUT_TRIGGER_TO_OFF_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//��ת
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_HOLD;//�Ժ�ʹ��ڱ���״̬
			}
			//���ش���ģʽ
			if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//�Ժ�ʹ��ڱ���״̬
			} else if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && input_trigger_state[i] == TRIGGER_AFTER) {
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//�Ժ�ʹ��ڱ���״̬
			}
		}
		if(input_trig_before_delay[i] > 0) {
			input_trig_before_delay[i] -= 10;  //10ms
		}
	}
}



//�������µ����봦�����
//------------------------------------------------------------------------------------------------


extern unsigned char switch_signal_hold_time[32];
extern unsigned char io_out[32/8];
unsigned char io_input_on_msk[32/8] = {0xFF,0xFF,0xFF,0xFF};

unsigned char io_pluase_time_count = 80;

//�ṩ��׼�Ľӿڣ����ڴ���IO_OUT��ʱ��
//����Ŀǰ��·��
unsigned int io_out_signal_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	uint32_t tmp;
	_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//���������������
	if(startbits >= tmp || bitcount == 0) {
		return 0;
	}
	//��һ���ж��Ƿ��������
	if((tmp - startbits) < bitcount) {
		bitcount = tmp - startbits;
	}
	//��ʼ����
	index = 0;
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(iobits[Bb]&code_msk[Bi]) {
		    switch_signal_hold_time[i] = io_pluase_time_count;
		}
		index++;
	}
	NutEventPost(&(sys_varient.io_out_event));
	return bitcount;
}

//�ṩ��׼�Ľӿڣ���������IO_OUT
//����Ŀǰ��·��
unsigned int io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;
	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//���������������
	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//��һ���ж��Ƿ��������
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//��ʼ����
	//printf("io_out_set_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	index = 0;
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(iobits[Bb]&code_msk[Bi]) {
			io_out[i/8] |=  code_msk[i%8];
			switch_signal_hold_time[i] = io_pluase_time_count;
		} else {
			io_out[i/8] &= ~code_msk[i%8];
			switch_signal_hold_time[i] = 0;
		}
		index++;
	}
	NutEventPost(&(sys_varient.io_out_event));
	return bitcount;
}
//�ṩ��׼�Ľӿڣ����ڷ�תIO_OUT
//����Ŀǰ��·��
unsigned int io_out_convert_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;
	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);

	//printf("get out num = %d\r\n",OUTPUT_CHANNEL_NUM);
	//���������������
	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//��һ���ж��Ƿ��������
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_out_convert_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//��ʼ����
	index = 0;
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(iobits[Bb]&code_msk[Bi]) {
			io_out[i/8] ^= code_msk[i%8];
			if(io_out[i/8]&code_msk[i%8]) {
				switch_signal_hold_time[i] = io_pluase_time_count;
			} else {
				switch_signal_hold_time[i] = 0;
			}
		}
		index++;
	}
	NutEventPost(&(sys_varient.io_out_event));
	return bitcount;
}
//�ṩ��׼�Ľӿڣ����ڶ�ȡIO_OUT
//����Ŀǰ��·��
unsigned int io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;

	memset(iobits,0,(bitcount+7)/8);

	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//���������������

	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//��һ���ж��Ƿ��������
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_out_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//��ʼ����
	index = 0;
	
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(io_out[i/8]&code_msk[i%8]) {
			iobits[Bb] |=  code_msk[Bi];
		} else {
			iobits[Bb] &= ~code_msk[Bi];
		}
		index++;
	}
	return bitcount;
}
unsigned int io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	unsigned char buffer[2];
	//uint32_t tmp;

	memset(iobits,0,(bitcount+7)/8);

	//_ioctl(_fileno(sys_varient.iofile), GET_IN_NUM, &tmp);
	//���������������
	if(startbits >= INPUT_CHANNEL_NUM || bitcount == 0) {
		//printf("io_in_get bit param error1:startbits = %d , bit count = %d\r\n",startbits,bitcount);
		return 0;
	}
	//��һ���ж��Ƿ��������
	if((INPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = INPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_in_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//��ʼ����
	index = 0;
	//
	_ioctl(_fileno(sys_varient.iofile), IO_IN_GET, buffer);
	
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(buffer[i/8]&code_msk[i%8]) {
			iobits[Bb] |=  code_msk[Bi];
		} else {
			iobits[Bb] &= ~code_msk[Bi];
		}
		index++;
	}
	return bitcount;
}

#if 0
void SetInputOnMsk(uint32_t input_on_msk,uint32_t input_off_msk)
{
	io_input_on_msk[0] |= (uint8_t)((input_on_msk >> 0)&0xFF);
	io_input_on_msk[1] |= (uint8_t)((input_on_msk >> 8)&0xFF);
	io_input_on_msk[2] |= (uint8_t)((input_on_msk >> 16)&0xFF);
	io_input_on_msk[3] |= (uint8_t)((input_on_msk >> 24)&0xFF);

	io_input_on_msk[0] &= ~(uint8_t)((input_off_msk >> 0)&0xFF);
	io_input_on_msk[1] &= ~(uint8_t)((input_off_msk >> 8)&0xFF);
	io_input_on_msk[2] &= ~(uint8_t)((input_off_msk >> 16)&0xFF);
	io_input_on_msk[3] &= ~(uint8_t)((input_off_msk >> 24)&0xFF);
}


void GetInputOnMsk(uint32_t * input_on_msk,uint32_t * input_off_msk)
{
	*input_on_msk    =  (uint8_t)io_input_on_msk[3];
	*input_on_msk  <<= 8;
	*input_on_msk   |=  (uint8_t)io_input_on_msk[2];
	*input_on_msk  <<= 8;
	*input_on_msk   |=  (uint8_t)io_input_on_msk[1];
	*input_on_msk  <<= 8;
	*input_on_msk   |=  (uint8_t)io_input_on_msk[0];
	//
	*input_off_msk   = (uint8_t)~io_input_on_msk[3];
	*input_off_msk <<= 8;
	*input_off_msk  |= (uint8_t)~io_input_on_msk[2];
	*input_off_msk <<= 8;
	*input_off_msk  |= (uint8_t)~io_input_on_msk[1];
	*input_off_msk <<= 8;
	*input_off_msk  |= (uint8_t)~io_input_on_msk[0];
}
#endif

void BspIoInInit(void)
{
	unsigned char i;
	for(i=0;i<IO_IN_COUNT_MAX;i++) {
		input_hold[i] = 0xFF;
	}
	memset(input_flag,0,sizeof(input_flag));
	memset(input_filter,0,sizeof(input_filter));
}


void GetFilterInputServer(unsigned char * buffer,uint32_t inputnum)
{
	
	unsigned char i;

	for(i=0;i<inputnum;i++) {
		unsigned char by = i / 8;
		unsigned char bi = i % 8;
		unsigned char msk = code_msk[bi];
		if(buffer[by]&msk) {
			if(input_flag[by]&msk) {
				//����
				if(input_hold[i] < 0xFF) {
					++input_hold[i];
				}
				if(input_hold[i] == INPUT_HOLD_COUNT) {
					input_filter[by] |=  msk;
				}
			} else {
				//�հ���
				input_flag[by] |= msk;
				input_hold[i] = 0;
			}
		} else {
			if(input_flag[by]&msk) {
				//���ɿ�
				input_flag[by] &= ~msk;
				input_hold[i] = 0;
			} else {
				//����
				if(input_hold[i] < 0xFF) {
					++input_hold[i];
				}
				if(input_hold[i] == INPUT_HOLD_COUNT) {
					input_filter[by] &= ~msk;
				}
			}
		}
	}
}

void SetRelayOneBitWithDelay(unsigned char index)
{
  if(index < IO_OUT_COUNT_MAX) {
    switch_signal_hold_time[index] = 80;
	io_out[index/8] |=  code_msk[index%8];
  }
}

void SetRelayWithDelay(uint32_t out)
{
	uint8_t i;
	for(i=0;i<IO_OUT_COUNT_MAX;i++) {
		if(out&0x01) {
			switch_signal_hold_time[i] = 80;
			io_out[i/8] |=  code_msk[i%8];
		} else {
			switch_signal_hold_time[i] = 0;
			io_out[i/8] &= ~code_msk[i%8];
		}
		out >>= 1;
	}
}
void RevertRelayWithDelay(uint32_t out)
{
	uint8_t i;
	for(i=0;i<IO_OUT_COUNT_MAX;i++) {
		if(out&0x01) {
			//if(THISINFO)printf("Rev Io %d,io_out[1] = 0x%x\r\n",i,io_out[1]);
			switch_signal_hold_time[i] = 80;
			io_out[i/8] ^=  (uint8_t)code_msk[i%8];
		}
		out >>= 1;
	}
}

void ClrRelayOneBitWithDelay(unsigned char index)
{
  if(index < IO_OUT_COUNT_MAX) {
    switch_signal_hold_time[index] = 0;
  }
  io_out[index/8] &= ~code_msk[index%8];
}
void RevertRelayOneBitWithDelay(unsigned char index)
{
  if(index < IO_OUT_COUNT_MAX) {
    switch_signal_hold_time[index] = 80;
	io_out[index/8] ^=  code_msk[index%8];
  }
}

unsigned char GetInputCtrlMode(unsigned char index)
{
	if(index < IO_OUT_COUNT_MAX) {
		return switch_input_control_mode[index];
	} else {
		return INPUT_TRIGGER_OFF_MODE;
	}
}

void IoInputToControlIoOutServer(void)
{
	unsigned char i;
  for(i=0;i<IO_IN_COUNT_MAX;i++) {
	  unsigned char by = i / 8;
	  unsigned char bi = i % 8;
	  unsigned char msk = code_msk[bi];
      //����ûһ·
	  if(io_input_on_msk[by]&msk) { //�����·���ܿصġ�
		  if(input_filter[by]&msk) { //������������ġ�
			  if(last_input[by]&msk) { //��һ��������
	              //���뱣��
	              switch(GetInputCtrlMode(i))  //����ģʽ�仯
	              {
	              case INPUT_TRIGGER_TO_OFF_MODE:
	              case INPUT_TRIGGER_TO_OPEN_MODE:
	              case INPUT_TRIGGER_FLIP_MODE:
	              case INPUT_SINGLE_TRIGGER_MODE:
				  case INPUT_EDGE_TRIG_MODE:
					  break;
				  case INPUT_LEVEL_CTL_ON_MODE:
					  SetRelayOneBitWithDelay(i);
					  break;
				  case INPUT_LEVEL_CTL_OFF_MODE:
	                  ClrRelayOneBitWithDelay(i);
	                  break;
	              default:
	                  break;
	              }
	            } else { //����
	              last_input[by] |= msk;
	              switch(GetInputCtrlMode(i))
	              {
	              case INPUT_SINGLE_TRIGGER_MODE:
					  if(THISINFO)printf("-------KEY OPEN ONT BIT----\r\n");
	                  SetRelayOneBitWithDelay(i);
	                break;
	              case INPUT_TRIGGER_TO_OFF_MODE:
					  ClrRelayOneBitWithDelay(i);
					  break;
	              case INPUT_TRIGGER_TO_OPEN_MODE:
					  SetRelayOneBitWithDelay(i);
	                  break;
	              case INPUT_TRIGGER_FLIP_MODE:
			      case INPUT_EDGE_TRIG_MODE:
					  if(THISINFO)printf("-------KEY VERT ONT BIT----\r\n");
					  RevertRelayOneBitWithDelay(i);
	                  break;
				  case INPUT_LEVEL_CTL_ON_MODE:
				  case INPUT_LEVEL_CTL_OFF_MODE:
	              default: 
	                  break;
	              }
	            }
              } else {
	            if(last_input[by]&msk) { //̧��
	              last_input[by] &= ~msk;
	              switch(GetInputCtrlMode(i))
	              {
	              case INPUT_TRIGGER_FLIP_MODE:
	              case INPUT_SINGLE_TRIGGER_MODE:
	              case INPUT_TRIGGER_TO_OFF_MODE:
	              case INPUT_TRIGGER_TO_OPEN_MODE:
					  break;
  				  case INPUT_EDGE_TRIG_MODE:
					  RevertRelayOneBitWithDelay(i);
	                  break;
				  case INPUT_LEVEL_CTL_ON_MODE:
				  case INPUT_LEVEL_CTL_OFF_MODE:
	              default:
	                break;
	              }
	            } else {
	              //�뿪����
	              switch(GetInputCtrlMode(i))
	              {
	              case INPUT_TRIGGER_FLIP_MODE:
	              case INPUT_SINGLE_TRIGGER_MODE:
	              case INPUT_TRIGGER_TO_OFF_MODE:
	              case INPUT_TRIGGER_TO_OPEN_MODE:
				  case INPUT_EDGE_TRIG_MODE:
					  break;
				  case INPUT_LEVEL_CTL_ON_MODE:
					  ClrRelayOneBitWithDelay(i);
					  break;
				  case INPUT_LEVEL_CTL_OFF_MODE:
					  SetRelayOneBitWithDelay(i);
					  break;
	              default:
	                break;
	              }
	            }
		  }
	  }
  }
}


void IoOutTimeTickUpdateServer(void)
{
  unsigned char i;
  for(i=0;i<IO_OUT_COUNT_MAX;i++) {
    if(GetInputCtrlMode(i) != INPUT_SINGLE_TRIGGER_MODE) {
	  continue;
	}
    if(switch_signal_hold_time[i] > 0) {
	   --switch_signal_hold_time[i];
	}
	if(switch_signal_hold_time[i] == 0) {
	  io_out[i/8] &= ~code_msk[i%8];
	}
  }
}


extern void check_app(void);
extern void check_get_mac_address(unsigned char * mac);

#if  1
//Ӧ�ͻ�����ʵ�������뷨
#if 0
8·ȫ����ȫ�ع��ܶ��壺

1����λ���������ȫ����ȫ�ع��ܲ��漰ʱ�����⡣����˵�������λ�������Ҫʱ�򿪺͹أ�������λ�����ʵ�֡�

2�����ȫ����ȫ�ض�Ӧ����˿ڵĵ�7����8·��������ơ�

3������7·���ź����룬�̵�������ʱ�򿪵Ĺ��ܡ����ܵ�8·�Ƿ����ź�

3������7·�źű������룬��Ӱ�쿪��״̬��

4������7·�źų�ȥ�źţ���Ӱ�쿪��״̬��

5������8·���ź����룬�̵�������ʱ��صĹ��ܡ����ܵ�7·�Ƿ����ź�

6������8·�źű������룬��Ӱ�쿪��״̬��

7������8·�źų�ȥ�źţ���Ӱ�쿪��״̬��

8: ����7·��8·ͬʱ���ź����룬����ʱ��ع��ܡ�

9����·�������ź���ʱ�˲�������100�����ڣ�С��100����ĸ����źţ���Ϊ���ź����롣
#endif
#endif


extern void io_scan_timing_server(void);

void io_out_ctl_thread_server(void)
{
	uint16_t count = 0;
	uint8_t  led = 0;


	NutSleep(5);

#ifdef APP_TIMEIMG_ON
	timing_init();
#endif

#if 0
	count = io_in_get_bits(0,input_flag,32);
	memcpy(last_input,input_flag,count);
	memcpy(input_filter,input_flag,count);
#endif

	if(THISINFO)printf("start scan io_in(0x%x),io_out(0x%x).\r\n",(unsigned int)last_input[0],io_out[0]);

	input_power_on_init();

    while(1) {
#ifdef APP_TIMEIMG_ON
		io_scan_timing_server();
#endif

		/*
#if 0
		if(1) {//�������
		    _ioctl(_fileno(sys_varient.iofile), IO_IN_GET, buffer);

			io_in_8ch = buffer[0] & (0x3<<6);

	        GetFilterInputServer(buffer,innum);
	        IoInputToControlIoOutServer();
			//Ӧ�ͻ�����������ָ����7·�������ȫ������8·�������ȫ��
#if 0   //�ͻ�ָ����ʱ�򿪺�ʱ���ܣ�����ֱ�����Σ����߲���Ҫ������ܡ�
			if(io_in_8ch_last != io_in_8ch) {
				unsigned char diff = io_in_8ch_last ^ io_in_8ch;
				if(diff == (0x3<<6)) { //�������仯
					if(io_in_8ch & (0x3<<6)) { //����ͬʱ�����봥���ź�
						if(timing_open_off_count <= 128) {
						    timing_open_off_count = 128+8;
							timing_delay_count = 0;
							DEBUGMSG(THISINFO,("close2 timing all close\r\n"));
						}
					}
				} else if(diff & (1<<6)) { //��7·��ͬ
					if(io_in_8ch & (1<<6)) {  //��8·�д����ź�,ȫ��
						if(timing_open_off_count >= 128) {
						    timing_open_off_count = 128-8;
							timing_delay_count = 0;
							DEBUGMSG(THISINFO,("open timing all open\r\n"));
						}
					}
				} else if(diff & (1<<7)) {//��8·��ͬ
					if(io_in_8ch & (1<<7)) {  //��8·�д����ź�,ȫ��
						if(timing_open_off_count <= 128) {
						    timing_open_off_count = 128+8;
							timing_delay_count = 0;
							DEBUGMSG(THISINFO,("close timing all close\r\n"));
						}
					}
				} else {
				}
				io_in_8ch_last = io_in_8ch;
			} else {
				//�ޱ仯
			}
			//���ݱ仯����
			if(timing_open_off_count != 128) {
				//���ݷ��򣬶���
				
				if(timing_open_off_count < 128) {
					unsigned char buffer[2];
					unsigned char diff = 128 - timing_open_off_count - 1;
					diff = 7 - diff;
					//ʱ��
					buffer[0] = (unsigned char)(diff & 0xFF);
					buffer[1] = (unsigned char)(diff>>8);
					_ioctl(_fileno(sys_varient.iofile), IO_SET_ONEBIT, buffer);
					if(++timing_delay_count >= 100) {
					    timing_open_off_count++;
						timing_delay_count = 0;
						DEBUGMSG(THISINFO,("             set bit ...\r\n"));
					}
				} else if(timing_open_off_count > 128) {
					unsigned char buffer[2];
					unsigned char diff = timing_open_off_count - 128 - 1;
					//diff = 7 - diff;
					//ʱ���
					buffer[0] = (unsigned char)(diff & 0xFF);
					buffer[1] = (unsigned char)(diff>>8);
					_ioctl(_fileno(sys_varient.iofile), IO_CLR_ONEBIT, buffer);
					if(++timing_delay_count >= 100) {
					    timing_open_off_count--;
						timing_delay_count = 0;
						DEBUGMSG(THISINFO,("             clr bit ...\r\n"));
					}
				}
			} else {
				timing_delay_count = 0;
			}
#endif
		}

		IoOutTimeTickUpdateServer();
#endif
		*/

#if (INPUT_CHANNEL_NUM > 0)
		get_filter_input_flag();
		trigger_timeout_handle();
#endif


		//�ȴ�10ms����
		NutSemWait(&sys_10ms_sem);
		//
		++count;
		if(count % 10 == 0) {
			//led ^= 0x01;
		}
		if(count < 180) {
			led |=  0x02;
		} else if(count < 200) {
			led &= ~0x02;
		} else {
			//unsigned char mac[6];
			count = 0;
			//check_app();
			//check_get_mac_address(mac);
		}
		BspDebugLedSet(led);
	}
}

THREAD(io_out_ctl_thread, arg)
{
	NutThreadSetPriority(IO_AND_TIMING_SCAN_RPI);
    while(1)io_out_ctl_thread_server();
}

void StartIoOutControlSrever(void)
{
    NutThreadCreate("io_out_ctl_thread",  io_out_ctl_thread, 0, 780);
}


