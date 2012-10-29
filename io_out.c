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
//重新修改输入触发的代码
//输入触发，简单有输入：延时触发时间，触发延时时间
//输入触发模式每种动作都有时间在里面
//默认上电后是不动作的。只有运行的时候发生了改变才需要触发，意思是说上电后初始数据需要根据输入的状态来初始化
//unsigned char input_trigger_signals[BITS_TO_BS(INPUT_CHANNEL_NUM)]; //输入各种模式的标志
unsigned char input_current_flag[BITS_TO_BS(INPUT_CHANNEL_NUM)];
unsigned char input_trigger_state[INPUT_CHANNEL_NUM]; //触发状态
unsigned int  input_filter_hold_time[INPUT_CHANNEL_NUM],input_filter_hold_time_max[INPUT_CHANNEL_NUM];
unsigned int  input_trig_before_delay[INPUT_CHANNEL_NUM];
unsigned int  input_trig_before_delay_max[INPUT_CHANNEL_NUM],input_trig_after_delay_max[INPUT_CHANNEL_NUM];
unsigned char input_trig_mode[INPUT_CHANNEL_NUM];
unsigned char input_trig_to_witch_io_out[INPUT_CHANNEL_NUM]; //需要触发那一路？只能映射一路

#define TRIGGER_NONE       0
#define TRIGGER_BEFORE     1
#define TRIGGER_HOLD       2
#define TRIGGER_AFTER      3





//上电初始化所有数据
//外部条件就是，必须等键盘程序已经准备好了
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

//实时地更新键盘键值，滤波输入，然后产生触发信号
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
				//保持
				//输入电平控制开模式
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_ON_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
				//输入电平控制关模式
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_OFF_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
			} else {
				//上升沿
				if(input_filter_hold_time[i] < input_filter_hold_time_max[i]) {
					input_filter_hold_time[i]++;
				} else {
				    input_current_flag[p] |=  code_msk[b];
				    //动作
				    //单触发
				    if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_AFTER)) {
					    //单触发模式下前延时后动作
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}
					//触发翻转延时到
					if(input_trig_mode[i] == INPUT_TRIGGER_FLIP_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}
					//触发开通模式
					if(input_trig_mode[i] == INPUT_TRIGGER_TO_OPEN_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
					//触发关闭模式
					if(input_trig_mode[i] == INPUT_TRIGGER_TO_OFF_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
					//边沿触发模式
					if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && 
						(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_AFTER)) { 
							//边沿触发，只有进入到保持状态，才能进行上升沿触发
						input_trig_before_delay[i] = input_trig_before_delay[i];
						input_trigger_state[i] = TRIGGER_BEFORE;
					}

				}
			}
		} else {
			if(input_current_flag[p]&code_msk[b]) {
				//下降沿
				if(input_filter_hold_time[i] > 0) {
					input_filter_hold_time[i] -= 10;
				} else {
					input_current_flag[p] &= ~code_msk[b];
					//动作
					//边沿触发模式
					if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && 
						(input_trigger_state[i] == TRIGGER_BEFORE || input_trigger_state[i] == TRIGGER_HOLD)) { 
						//边沿触发，只有进入到保持状态，才能进行下降沿触发
						input_trig_before_delay[i] = input_trig_after_delay_max[i];
						input_trigger_state[i] = TRIGGER_AFTER;
					}
				}
			} else {
				//空闲
				input_filter_hold_time[i] = 0;
				//保持
				//输入电平控制开模式
				if(input_trig_mode[i] == INPUT_LEVEL_CTL_ON_MODE && 
					(input_trigger_state[i] == TRIGGER_NONE || input_trigger_state[i] == TRIGGER_HOLD)) {
					io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
					input_trigger_state[i] = TRIGGER_HOLD;
				}
				//输入电平控制关模式
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
#define INPUT_TRIGGER_FLIP_MODE          0x00  //触发反转模式
#define INPUT_SINGLE_TRIGGER_MODE        0x01  //单触发模式
#define INPUT_TRIGGER_TO_OPEN_MODE       0x02  //触发开通模式
#define INPUT_TRIGGER_TO_OFF_MODE        0x03  //触发关闭模式
#define INPUT_EDGE_TRIG_MODE             0x04  //边沿触发模式
#define INPUT_LEVEL_CTL_ON_MODE          0x05  //输入电平控制开模式
#define INPUT_LEVEL_CTL_OFF_MODE         0x06  //输入电平控制关模式
#define INPUT_TRIGGER_OFF_MODE           0x07  //控制关闭
/*
*/
#endif

//触发延时动作
void trigger_timeout_handle(void)
{
	unsigned char reg0x1 = 0x01;
	unsigned char reg0x0 = 0x00;
	unsigned int i;
	for(i=0;i<INPUT_CHANNEL_NUM;i++) {
		//什么状态和什么模式就执行什么动作
		if(input_trig_before_delay[i] == 0) {
			//单触发
		    if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//单触发模式下前延时后动作
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trig_before_delay[i] = input_trig_after_delay_max[i];
				input_trigger_state[i] = TRIGGER_HOLD;
		    } else if(input_trig_mode[i] == INPUT_SINGLE_TRIGGER_MODE && input_trigger_state[i] == TRIGGER_HOLD) {
				//单触发模式下前延时后动作
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_AFTER;
			}
			//触发翻转延时到
			if(input_trig_mode[i] == INPUT_TRIGGER_FLIP_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//翻转
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//以后就处于保持状态
			}
			//触发开通模式
			if(input_trig_mode[i] == INPUT_TRIGGER_TO_OPEN_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//翻转
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_HOLD;//以后就处于保持状态
			}
			//触发关闭模式
			if(input_trig_mode[i] == INPUT_TRIGGER_TO_OFF_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				//翻转
				io_out_set_bits(input_trig_to_witch_io_out[i],&reg0x0,1);
				input_trigger_state[i] = TRIGGER_HOLD;//以后就处于保持状态
			}
			//边沿触发模式
			if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && input_trigger_state[i] == TRIGGER_BEFORE) {
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//以后就处于保持状态
			} else if(input_trig_mode[i] == INPUT_EDGE_TRIG_MODE && input_trigger_state[i] == TRIGGER_AFTER) {
				io_out_convert_bits(input_trig_to_witch_io_out[i],&reg0x1,1);
				input_trigger_state[i] = TRIGGER_HOLD;//以后就处于保持状态
			}
		}
		if(input_trig_before_delay[i] > 0) {
			input_trig_before_delay[i] -= 10;  //10ms
		}
	}
}



//以上是新的输入处理代码
//------------------------------------------------------------------------------------------------


extern unsigned char switch_signal_hold_time[32];
extern unsigned char io_out[32/8];
unsigned char io_input_on_msk[32/8] = {0xFF,0xFF,0xFF,0xFF};

unsigned char io_pluase_time_count = 80;

//提供标准的接口，用于触发IO_OUT的时间
//返回目前的路数
unsigned int io_out_signal_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	uint32_t tmp;
	_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//参数必须符合条件
	if(startbits >= tmp || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((tmp - startbits) < bitcount) {
		bitcount = tmp - startbits;
	}
	//开始设置
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

//提供标准的接口，用于设置IO_OUT
//返回目前的路数
unsigned int io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;
	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//参数必须符合条件
	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//开始设置
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
//提供标准的接口，用于翻转IO_OUT
//返回目前的路数
unsigned int io_out_convert_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;
	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);

	//printf("get out num = %d\r\n",OUTPUT_CHANNEL_NUM);
	//参数必须符合条件
	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_out_convert_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//开始设置
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
//提供标准的接口，用于读取IO_OUT
//返回目前的路数
unsigned int io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;

	memset(iobits,0,(bitcount+7)/8);

	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//参数必须符合条件

	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_out_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//开始设置
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
	//参数必须符合条件
	if(startbits >= INPUT_CHANNEL_NUM || bitcount == 0) {
		//printf("io_in_get bit param error1:startbits = %d , bit count = %d\r\n",startbits,bitcount);
		return 0;
	}
	//进一步判断是否符合条件
	if((INPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = INPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_in_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//开始设置
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
				//保持
				if(input_hold[i] < 0xFF) {
					++input_hold[i];
				}
				if(input_hold[i] == INPUT_HOLD_COUNT) {
					input_filter[by] |=  msk;
				}
			} else {
				//刚按下
				input_flag[by] |= msk;
				input_hold[i] = 0;
			}
		} else {
			if(input_flag[by]&msk) {
				//刚松开
				input_flag[by] &= ~msk;
				input_hold[i] = 0;
			} else {
				//空闲
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
      //遍历没一路
	  if(io_input_on_msk[by]&msk) { //如果此路是受控的。
		  if(input_filter[by]&msk) { //如果输入是正的。
			  if(last_input[by]&msk) { //上一次是正的
	              //输入保持
	              switch(GetInputCtrlMode(i))  //根据模式变化
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
	            } else { //按下
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
	            if(last_input[by]&msk) { //抬起
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
	              //离开保持
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
//应客户需求，实现以下想法
#if 0
8路全开与全关功能定义：

1：上位机软件对于全开和全关功能不涉及时序问题。即是说，如果上位机软件需要时序开和关，必须上位机软件实现。

2：输出全开和全关对应输入端口的第7、第8路的输入控制。

3：当第7路有信号输入，继电器发生时序开的功能。不管第8路是否有信号

3：当第7路信号保持输入，不影响开关状态。

4：当第7路信号撤去信号，不影响开关状态。

5：当第8路有信号输入，继电器发生时序关的功能。不管第7路是否有信号

6：当第8路信号保持输入，不影响开关状态。

7：当第8路信号撤去信号，不影响开关状态。

8: 当第7路第8路同时有信号输入，发生时序关功能。

9：两路的输入信号延时滤波控制在100毫秒内，小于100毫秒的干扰信号，认为无信号输入。
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
		if(1) {//输入控制
		    _ioctl(_fileno(sys_varient.iofile), IO_IN_GET, buffer);

			io_in_8ch = buffer[0] & (0x3<<6);

	        GetFilterInputServer(buffer,innum);
	        IoInputToControlIoOutServer();
			//应客户需求，在这里指定第7路输入控制全开，第8路输入控制全关
#if 0   //客户指定的时序开和时序功能，这里直接屏蔽，主线不需要这个功能。
			if(io_in_8ch_last != io_in_8ch) {
				unsigned char diff = io_in_8ch_last ^ io_in_8ch;
				if(diff == (0x3<<6)) { //两个都变化
					if(io_in_8ch & (0x3<<6)) { //两个同时又输入触发信号
						if(timing_open_off_count <= 128) {
						    timing_open_off_count = 128+8;
							timing_delay_count = 0;
							DEBUGMSG(THISINFO,("close2 timing all close\r\n"));
						}
					}
				} else if(diff & (1<<6)) { //第7路不同
					if(io_in_8ch & (1<<6)) {  //第8路有触发信号,全开
						if(timing_open_off_count >= 128) {
						    timing_open_off_count = 128-8;
							timing_delay_count = 0;
							DEBUGMSG(THISINFO,("open timing all open\r\n"));
						}
					}
				} else if(diff & (1<<7)) {//第8路不同
					if(io_in_8ch & (1<<7)) {  //第8路有触发信号,全关
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
				//无变化
			}
			//根据变化动作
			if(timing_open_off_count != 128) {
				//根据方向，动作
				
				if(timing_open_off_count < 128) {
					unsigned char buffer[2];
					unsigned char diff = 128 - timing_open_off_count - 1;
					diff = 7 - diff;
					//时序开
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
					//时序关
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


		//等待10ms到来
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


