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
#include "debug.h"


#define THISINFO          1
#define THISERROR         1


#define  IO_OUT_COUNT_MAX            32
#define  IO_IN_COUNT_MAX             32
#define  INPUT_HOLD_COUNT            5  //

//
const uint32_t  code_msk[32] = {0x01,0x02,0x04,0x08,0x10UL,0x20UL,0x40UL,0x80UL,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000UL,
                                0x10000UL,0x20000UL,0x40000UL,0x80000UL,0x100000UL,0x200000UL,0x400000UL,0x800000UL,0x1000000UL,0x2000000UL,0x4000000UL,0x8000000UL,
                                0x10000000UL,0x20000000UL,0x40000000UL,0x80000000UL};

uint32_t      last_input                        __attribute__ ((section (".noinit")));
unsigned char input_flag[IO_IN_COUNT_MAX/8]     __attribute__ ((section (".noinit")));
unsigned char input_filter[IO_IN_COUNT_MAX/8]   __attribute__ ((section (".noinit")));
unsigned char input_hold[IO_IN_COUNT_MAX]       __attribute__ ((section (".noinit")));
unsigned char switch_input_control_mode[IO_IN_COUNT_MAX];



extern unsigned char switch_signal_hold_time[32];
extern unsigned char io_out[32/8];
unsigned char io_input_on_msk[32/8] = {0xFF,0xFF,0xFF,0xFF};

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


void BspIoInInit(void)
{
	unsigned char i;
	for(i=0;i<IO_IN_COUNT_MAX;i++) {
		input_hold[i] = 0xFF;
	}
	input_flag[0] = 0;
	input_filter[0] = 0;
}

void BspIoOutInit(void)
{
#if 0  //IO_OUT 不在这这里干了
	uint8_t i;
	for(i=0;i<IO_OUT_COUNT_MAX/8;i++) {
		switch_signal_hold_time[i] = 0;
#ifdef DEFINE_POWER_RELAY_OLL_ON
		io_out[i] = 0xFF;
#else
		io_out[i] = 0;
#endif
	}
#endif

}

uint32_t GetIoOut(void)
{
	uint32_t out = io_out[3];
	out <<= 8;
	out |= io_out[2];
	out <<= 8;
	out |= io_out[1];
	out <<= 8;
	out |= io_out[0];
	return out;
}

uint32_t GetFilterInput(void)
{
	uint32_t out = input_filter[0];
	return out;
}

void GetFilterInputServer(unsigned char * buffer,uint32_t inputnum)
{
	
	unsigned char i;
	//uint32_t last_input = group_arry4_to_uint32(input_filter);
	uint32_t input  = group_arry4_to_uint32(buffer);

	for(i=0;i<inputnum;i++) {
		if(input&code_msk[i]) {
			if(input_flag[i/8]&code_msk[i%8]) {
				//保持
				if(input_hold[i] < 0xFF) {
					++input_hold[i];
				}
				if(input_hold[i] == INPUT_HOLD_COUNT) {
					input_filter[0] |=  code_msk[i];
				}
			} else {
				//刚按下
				input_flag[0] |= code_msk[i];
				input_hold[i] = 0;
			}
		} else {
			if(input_flag[0]&code_msk[i]) {
				//刚松开
				input_flag[0] &= ~code_msk[i];
				input_hold[i] = 0;
			} else {
				//空闲
				if(input_hold[i] < 0xFF) {
					++input_hold[i];
				}
				if(input_hold[i] == INPUT_HOLD_COUNT) {
					input_filter[0] &= ~code_msk[i];
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


int IsNotSupportIndex(unsigned char index)
{
  if(index < IO_OUT_COUNT_MAX) {
    return -1;
  }
  return 0;
}

unsigned char GetInputCtrlMode(unsigned char index)
{
	unsigned char mode = INPUT_TRIGGER_OFF_MODE;
    if(!IsNotSupportIndex(index)) return INPUT_TRIGGER_OFF_MODE;
    mode = switch_input_control_mode[index];
    return mode;
}

void IoInputToControlIoOutServer(void)
{
  unsigned char i;
  uint32_t input = GetFilterInput();
  for(i=0;i<8;i++) {
      //遍历没一路
	  if(io_input_on_msk[0]&code_msk[i]) { //如果此路是受控的。
		  if(input&code_msk[i]) { //如果输入是正的。
			  if(last_input&code_msk[i]) { //上一次是正的
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
	              last_input |= code_msk[i];
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
	            if(last_input&code_msk[i]) { //抬起
	              last_input &= ~code_msk[i];
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

void io_out_ctl_thread_server(void)
{
	int rc = -1;
	uint32_t outnum,innum;
	uint16_t count = 0;
	uint8_t  led = 0;
	unsigned char buffer[16];
	unsigned char io_in_8ch,io_in_8ch_last;
	unsigned char timing_open_off_count = 128;  //如果小于128，则需要打开，如果大于128，则需要关闭
	unsigned char timing_delay_count = 0;


	rc = _ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &outnum);

	if(!rc) {
		if(THISINFO)printf("fopen relayctl succ.\r\n");
	} else {
		if(THISINFO)printf("fopen relayctl failed.\r\n");
	}


	_ioctl(_fileno(sys_varient.iofile), GET_IN_NUM, &innum);
	
	if(THISINFO)printf("IOCTL:inputnum(%d),outputnum(%d).\r\n",(int)innum,(int)outnum);

#ifdef APP_TIMEIMG_ON
	timing_init();
#endif

	_ioctl(_fileno(sys_varient.iofile), IO_IN_GET, buffer);
	io_in_8ch_last = io_in_8ch = input_filter[0] = input_flag[0] = last_input = buffer[0];
	
	if(THISINFO)printf("start scan io_in(0x%x),io_out(0x%x).\r\n",(unsigned int)last_input,io_out[0]);

    while(1) {
#ifdef APP_TIMEIMG_ON
		io_scan_timing_server();
#endif
		if(innum) {//输入控制
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
			unsigned char mac[6];
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


