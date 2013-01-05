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
#include <sys/atom.h>

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
#include "plc_io_inter.h"
#include "debug.h"

#include "plc_command_def.h"
#include "plc_prase.h"
#include "compiler.h"

#define THIS_INFO  1
#define THIS_ERROR 1


//100ms计时器的控制数据结构
typedef struct _TIM100MS_ARRAYS_T
{
    WORD  counter[TIMING100MS_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  enable_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  event_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
	BYTE  holding_bits[BITS_TO_BS(TIMING100MS_EVENT_COUNT)];
} TIM100MS_ARRAYS_T;

TIM100MS_ARRAYS_T  tim100ms_arrys;



//1s计时器的控制数据结构
typedef struct _TIM1S_ARRAYS_T
{
    WORD  counter[TIMING1S_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  enable_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  event_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
	BYTE  holding_bits[BITS_TO_BS(TIMING1S_EVENT_COUNT)];
} TIM1S_ARRAYS_T;

TIM1S_ARRAYS_T    tim1s_arrys;

//计数器的控制数据结构
typedef struct _COUNTER_ARRAYS_T
{
    WORD  counter[COUNTER_EVENT_COUNT];
	BYTE  upordown_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];		
	BYTE  event_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  event_bits_last[BITS_TO_BS(COUNTER_EVENT_COUNT)];
	BYTE  last_trig_bits[BITS_TO_BS(COUNTER_EVENT_COUNT)];
} COUNTER_ARRAYS_T;

COUNTER_ARRAYS_T counter_arrys;

//输入口
unsigned char inputs_new[BITS_TO_BS(IO_INPUT_COUNT)];
unsigned char inputs_last[BITS_TO_BS(IO_INPUT_COUNT)];
//输出继电器
unsigned char output_last[BITS_TO_BS(IO_OUTPUT_COUNT)]  __attribute__ ((section (".noinit")));
unsigned char output_new[BITS_TO_BS(IO_OUTPUT_COUNT)]   __attribute__ ((section (".noinit")));
//辅助继电器
unsigned char auxi_relays[BITS_TO_BS(AUXI_RELAY_COUNT)];
unsigned char auxi_relays_last[BITS_TO_BS(AUXI_RELAY_COUNT)];
//特殊寄存器
unsigned char speicial_relays[BITS_TO_BS(SPECIAL_RELAY_COUNT)];
unsigned char speicial_relays_last[BITS_TO_BS(SPECIAL_RELAY_COUNT)];
//保持寄存器影像，读再次，写包括写物理期间
unsigned char hold_register_map[BITS_TO_BS(AUXI_HOLDRELAY_COUNT)];
unsigned char hold_register_map_last[BITS_TO_BS(AUXI_HOLDRELAY_COUNT)];
unsigned char hold_durty = 0;
//字节变量，通用变量
unsigned char general_reg[REG_COUNT];   //普通变量
//RTC实时时间影像寄存器，没秒更新一次自动变化
unsigned char rtc_reg[REG_RTC_COUNT];
//温度寄存器，1秒更新一次
unsigned char temp_reg[REG_TEMP_COUNT];
//定时器定义，自动对内部的时钟脉冲进行计数
volatile unsigned int  time100ms_come_flag;
volatile unsigned int  time1s_come_flag;
//运算器的寄存器
#define  BIT_STACK_LEVEL     32
unsigned char bit_acc;
unsigned char bit_stack[BITS_TO_BS(BIT_STACK_LEVEL)];   //比特堆栈，PLC的位运算结果压栈在这里，总共有32层栈
unsigned char bit_stack_sp;   //比特堆栈的指针

//指令编码
unsigned int  plc_command_index;     //当前指令索引，
unsigned char plc_command_array[32]; //当前指令字节编码
#define       PLC_CODE     (plc_command_array[0])

//处理器状态
unsigned char plc_cpu_stop;
//通信元件个数
unsigned int  net_communication_count = 0;
unsigned int  net_global_send_index = 0; //发送令牌，指示下一个个发送的令牌

/*************************************************
 * 以下是私有的实现
 */
//内部用系统计数器
static uint32_t last_tick;
static uint32_t last_tick1s;
unsigned char secend_tick_come = 0;
unsigned char last_tm_sec;

void sys_time_tick_init(void)
{
	last_tick = NutGetMillis();
	last_tick1s = NutGetMillis();
}

void plc_rtc_tick_process(void)
{
	//读时间进来
	struct _tm  sys_time;
	NutRtcGetTime(&sys_time);
	//以下按照从小到大排序，分别为：秒，分，时，日，月，年，最后是星期
	sys_lock();
	rtc_reg[5] = sys_time.tm_year;
	rtc_reg[4] = sys_time.tm_mon;
	rtc_reg[3] = sys_time.tm_mday;
	rtc_reg[2] = sys_time.tm_hour;
	rtc_reg[1] = sys_time.tm_min;
	rtc_reg[0] = sys_time.tm_sec;
	rtc_reg[6] = sys_time.tm_wday;
	sys_unlock();
}

void plc_timing_tick_process(void)
{
	uint32_t curr = NutGetMillis();
	if((curr - last_tick) >= 100) {  //100ms //时间快了点，稍微增大计数器，反之减小计数
		//100ms
		sys_lock();
		time100ms_come_flag++;
		//1 sec
		if(last_tm_sec != rtc_reg[0]) {
	        last_tm_sec = rtc_reg[0];
		    time1s_come_flag++;
	    }
		last_tick += 100;
	    sys_unlock();
	}
	if((curr - last_tick1s) >= 1000) {
		last_tick1s += 1000;
		secend_tick_come = 1;
	}
}




/*********************************************
 * 系统初始化
 */

void memsetbit(unsigned char * des,unsigned char * src,unsigned int bitcount)
{
	unsigned int i;
	for(i=0;i<bitcount;i++) {
		SET_BIT(des,i,BIT_IS_SET(src,i));
	}
}

void PlcInit(void)
{
	phy_io_out_get_bits(0,output_new,IO_OUTPUT_COUNT);
	phy_io_in_get_bits(0,inputs_new,IO_INPUT_COUNT);
	memcpy(inputs_last,inputs_new,sizeof(inputs_new));

	if(get_reset_type() != WDT_RESET) { //不是看门狗复位，都必须初始化
	    memset(output_new,0,sizeof(output_new));
	    memset(output_last,0,sizeof(output_last));
		memset(auxi_relays,0,sizeof(auxi_relays));
		memset(auxi_relays_last,0,sizeof(auxi_relays_last));
	} else {
	    memcpy(output_last,output_new,sizeof(output_new));
		memsetbit(auxi_relays,output_new,IO_OUTPUT_COUNT);
		memcpy(auxi_relays_last,auxi_relays,sizeof(auxi_relays_last));
	}

	memset(speicial_relays,0,sizeof(speicial_relays));
	memset(speicial_relays_last,0,sizeof(speicial_relays_last));
	{ //复位状态简单的初始化为1，即表示上电标志（此位人工清零）
		SET_BIT(speicial_relays,0,1);
	}
	//初始化保持寄存器
	holder_register_read(0,hold_register_map,sizeof(hold_register_map));
	memcpy(hold_register_map_last,hold_register_map,sizeof(hold_register_map));
	//
	time100ms_come_flag = 0;
	time1s_come_flag = 0;
	memset(&tim100ms_arrys,0,sizeof(tim100ms_arrys));
	memset(&tim1s_arrys,0,sizeof(tim1s_arrys));
    memset(&counter_arrys,0,sizeof(counter_arrys));
	memset(general_reg,0,sizeof(general_reg));
	sys_time_tick_init();
	plc_rtc_tick_process();
	bit_acc = 0;
	memset(bit_stack,0,sizeof(bit_stack));
	bit_stack_sp = 0;
	plc_command_index = 0;
}


/*
    指令                      地址         数据
51 00 00 00 00 05 00  +   34 00 01 00  + 00  //0
51 00 00 00 00 05 00  +   35 00 01 00  + FF  //end  
51 00 00 00 00 07 00  +   36 00 03 00  + 01 00 00   //ld 0x00,0x00
51 00 00 00 00 07 00  +   39 00 03 00  + 04 01 00   //ld 0x00,0x00
51 00 00 00 00 05 00  +   42 00 01 00  + 14
51 00 00 00 00 07 00  +   43 00 03 00  + 04 01 01   //ld 0x00,0x00
51 00 00 00 00 05 00  +   46 00 01 00  + FF    //end

/-------------------+-----------+-- -- --
51 00 00 00 00 05 00 34 00 01 00 00 
51 00 00 00 00 05 00 35 00 01 00 FF 
51 00 00 00 00 05 00 35 00 01 00 00 

51 00 00 00 00 07 00 36 00 03 00 03 00 00
51 00 00 00 00 07 00 39 00 03 00 04 01 00
51 00 00 00 00 05 00 42 00 01 00 14
51 00 00 00 00 07 00 43 00 03 00 04 01 01
51 00 00 00 00 05 00 46 00 01 00 FF

/-------------------+-----------+-- -- --
50 00 00 00 00 05 00 34 00 14 00

50 00 00 01 00 18 00 34 00 14 00 00 03 00 00 04 01 00 14 04 01 01 ff 00 00 00 00 00 00 00 00 

03 08 00 
16 08 00 00 0A 
01 08 00 
23 01 00 
14 
23 01 01 
FF

*/


const unsigned char plc_test_flash[1024] =
{
	0,
#if 0  //客户指定1小时关的
	PLC_LDP, 0x00,0x00,
	PLC_SET, 0x02,50,
	PLC_SET, 0x02,0x00,
	PLC_LDF, 0x00,0x00,
	PLC_RST, 0x02,50,
	PLC_RST, 0x02,0x00,

	PLC_LDP, 0x00,0x01,
	PLC_SET, 0x02,51,
	PLC_SET, 0x02,0x01,
	PLC_LDF, 0x00,0x01,
	PLC_RST, 0x02,51,
	PLC_RST, 0x02,0x01,

	PLC_LDP, 0x00,0x02,
	PLC_SET, 0x02,52,
	PLC_SET, 0x02,0x02,
	PLC_LDF, 0x00,0x02,
	PLC_RST, 0x02,52,
	PLC_RST, 0x02,0x02,

	PLC_LDP, 0x00,0x03,
	PLC_SET, 0x02,53,
	PLC_SET, 0x02,0x03,
	PLC_LDF, 0x00,0x03,
	PLC_RST, 0x02,53,
	PLC_RST, 0x02,0x03,

	PLC_LDP, 0x00,0x04,
	PLC_SET, 0x02,54,
	PLC_SET, 0x02,0x04,
	PLC_LDF, 0x00,0x04,
	PLC_RST, 0x02,54,
	PLC_RST, 0x02,0x04,

	PLC_LDP, 0x00,0x05,
	PLC_SET, 0x02,55,
	PLC_SET, 0x02,0x05,
	PLC_LDF, 0x00,0x05,
	PLC_RST, 0x02,55,
	PLC_RST, 0x02,0x05,

	PLC_LDP, 0x00,0x06,
	PLC_SET, 0x02,56,
	PLC_SET, 0x02,0x06,
	PLC_LDF, 0x00,0x06,
	PLC_RST, 0x02,56,
	PLC_RST, 0x02,0x06,

	PLC_LDP, 0x00,0x07,
	PLC_SET, 0x02,57,
	PLC_SET, 0x02,0x07,
	PLC_LDF, 0x00,0x07,
	PLC_RST, 0x02,57,
	PLC_RST, 0x02,0x07,

	PLC_LDP, 0x00,0x08,
	PLC_SET, 0x02,58,
	PLC_SET, 0x02,0x08,
	PLC_LDF, 0x00,0x08,
	PLC_RST, 0x02,58,
	PLC_RST, 0x02,0x08,

	PLC_LD,  0x02,50,
	PLC_OUTT,0x0C,0x00,0x0E,0x10,
	PLC_LD,  0x02,51,
	PLC_OUTT,0x0C,0x01,0x0E,0x10,
	PLC_LD,  0x02,52,
	PLC_OUTT,0x0C,0x02,0x0E,0x10,  //最后两位是时间
	PLC_LD,  0x02,53,
	PLC_OUTT,0x0C,0x03,0x0E,0x10,
	PLC_LD,  0x02,54,
	PLC_OUTT,0x0C,0x04,0x0E,0x10,
	PLC_LD,  0x02,55,
	PLC_OUTT,0x0C,0x05,0x0E,0x10,
	PLC_LD,  0x02,56,
	PLC_OUTT,0x0C,0x06,0x0E,0x10,
	PLC_LD,  0x02,57,
	PLC_OUTT,0x0C,0x07,0x0E,0x10,

	PLC_LDP, 0x0C,0x00,
	PLC_RST, 0x02,0x00,

	PLC_LDP, 0x0C,0x01,
	PLC_RST, 0x02,0x01,

	PLC_LDP, 0x0C,0x02,
	PLC_RST, 0x02,0x02,

	PLC_LDP, 0x0C,0x03,
	PLC_RST, 0x02,0x03,

	PLC_LDP, 0x0C,0x04,
	PLC_RST, 0x02,0x04,

	PLC_LDP, 0x0C,0x05,
	PLC_RST, 0x02,0x05,

	PLC_LDP, 0x0C,0x06,
	PLC_RST, 0x02,0x06,

	PLC_LDP, 0x0C,0x07,
	PLC_RST, 0x02,0x07,

	PLC_LD,  0x02,0x00,
	PLC_OUT, 0x01,0x00,
	PLC_LD,  0x02,0x01,
	PLC_OUT, 0x01,0x01,
	PLC_LD,  0x02,0x02,
	PLC_OUT, 0x01,0x02,
	PLC_LD,  0x02,0x03,
	PLC_OUT, 0x01,0x03,
	PLC_LD,  0x02,0x04,
	PLC_OUT, 0x01,0x04,
	PLC_LD,  0x02,0x05,
	PLC_OUT, 0x01,0x05,
	PLC_LD,  0x02,0x06,
	PLC_OUT, 0x01,0x06,
	PLC_LD,  0x02,0x07,
	PLC_OUT, 0x01,0x07,
#endif

#if 0 //16路上电保持
	PLC_LDI, 0x06,0x00,
	PLC_JMPS,0x00,101,

	PLC_LD,  0x04,0x00,
	PLC_OUT, 0x02,0x00,
	PLC_LD,  0x04,0x01,
	PLC_OUT, 0x02,0x01,
	PLC_LD,  0x04,0x02,
	PLC_OUT, 0x02,0x02,
	PLC_LD,  0x04,0x03,
	PLC_OUT, 0x02,0x03,
	PLC_LD,  0x04,0x04,
	PLC_OUT, 0x02,0x04,
	PLC_LD,  0x04,0x05,
	PLC_OUT, 0x02,0x05,
	PLC_LD,  0x04,0x06,
	PLC_OUT, 0x02,0x06,
	PLC_LD,  0x04,0x07,
	PLC_OUT, 0x02,0x07,

	PLC_LD,  0x04,0x08,
	PLC_OUT, 0x02,0x08,
	PLC_LD,  0x04,0x09,
	PLC_OUT, 0x02,0x09,
	PLC_LD,  0x04,0x0a,
	PLC_OUT, 0x02,0x0a,
	PLC_LD,  0x04,0x0b,
	PLC_OUT, 0x02,0x0b,
	PLC_LD,  0x04,0x0c,
	PLC_OUT, 0x02,0x0c,
	PLC_LD,  0x04,0x0d,
	PLC_OUT, 0x02,0x0d,
	PLC_LD,  0x04,0x0e,
	PLC_OUT, 0x02,0x0e,
	PLC_LD,  0x04,0x0f,
	PLC_OUT, 0x02,0x0f,

	PLC_LDKL,
	PLC_OUT, 0x06,0x00,

	PLC_END,


	PLC_LD,  0x02,0x00,
	PLC_OUT, 0x01,0x00,
	PLC_OUT, 0x04,0x00,
	PLC_LD,  0x02,0x01,
	PLC_OUT, 0x01,0x01,
	PLC_OUT, 0x04,0x01,
	PLC_LD,  0x02,0x02,
	PLC_OUT, 0x01,0x02,
	PLC_OUT, 0x04,0x02,
	PLC_LD,  0x02,0x03,
	PLC_OUT, 0x01,0x03,
	PLC_OUT, 0x04,0x03,
	PLC_LD,  0x02,0x04,
	PLC_OUT, 0x01,0x04,
	PLC_OUT, 0x04,0x04,
	PLC_LD,  0x02,0x05,
	PLC_OUT, 0x01,0x05,
	PLC_OUT, 0x04,0x05,
	PLC_LD,  0x02,0x06,
	PLC_OUT, 0x01,0x06,
	PLC_OUT, 0x04,0x06,
	PLC_LD,  0x02,0x07,
	PLC_OUT, 0x01,0x07,
	PLC_OUT, 0x04,0x07,

	PLC_LD,  0x02,0x08,
	PLC_OUT, 0x01,0x08,
	PLC_OUT, 0x04,0x08,
	PLC_LD,  0x02,0x09,
	PLC_OUT, 0x01,0x09,
	PLC_OUT, 0x04,0x09,
	PLC_LD,  0x02,0x0a,
	PLC_OUT, 0x01,0x0a,
	PLC_OUT, 0x04,0x0a,
	PLC_LD,  0x02,0x0b,
	PLC_OUT, 0x01,0x0b,
	PLC_OUT, 0x04,0x0b,
	PLC_LD,  0x02,0x0c,
	PLC_OUT, 0x01,0x0c,
	PLC_OUT, 0x04,0x0c,
	PLC_LD,  0x02,0x0d,
	PLC_OUT, 0x01,0x0d,
	PLC_OUT, 0x04,0x0d,
	PLC_LD,  0x02,0x0e,
	PLC_OUT, 0x01,0x0e,
	PLC_OUT, 0x04,0x0e,
	PLC_LD,  0x02,0x0f,
	PLC_OUT, 0x01,0x0f,
	PLC_OUT, 0x04,0x0f,
#endif
#if 1 //输入控制输出
	PLC_LDP, 0x00,0x00,
	PLC_SET, 0x02,0x00,
	PLC_LDF, 0x00,0x00,
	PLC_RST, 0x02,0x00,
	PLC_LDP, 0x00,0x01,
	PLC_SET, 0x02,0x01,
	PLC_LDF, 0x00,0x01,
	PLC_RST, 0x02,0x01,
	PLC_LDP, 0x00,0x02,
	PLC_SET, 0x02,0x02,
	PLC_LDF, 0x00,0x02,
	PLC_RST, 0x02,0x02,
	PLC_LDP, 0x00,0x03,
	PLC_SET, 0x02,0x03,
	PLC_LDF, 0x00,0x03,
	PLC_RST, 0x02,0x03,
	PLC_LDP, 0x00,0x04,
	PLC_SET, 0x02,0x04,
	PLC_LDF, 0x00,0x04,
	PLC_RST, 0x02,0x04,
	PLC_LDP, 0x00,0x05,
	PLC_SET, 0x02,0x05,
	PLC_LDF, 0x00,0x05,
	PLC_RST, 0x02,0x05,
	PLC_LDP, 0x00,0x06,
	PLC_SET, 0x02,0x06,
	PLC_LDF, 0x00,0x06,
	PLC_RST, 0x02,0x06,
	PLC_LDP, 0x00,0x07,
	PLC_SET, 0x02,0x07,
	PLC_LDF, 0x00,0x07,
	PLC_RST, 0x02,0x07,
	PLC_LDP, 0x00,0x08,
	PLC_SET, 0x02,0x08,
	PLC_LDF, 0x00,0x08,
	PLC_RST, 0x02,0x08,
	PLC_LDP, 0x00,0x09,
	PLC_SET, 0x02,0x09,
	PLC_LDF, 0x00,0x09,
	PLC_RST, 0x02,0x09,
	PLC_LDP, 0x00,0x0a,
	PLC_SET, 0x02,0x0a,
	PLC_LDF, 0x00,0x0a,
	PLC_RST, 0x02,0x0a,
	PLC_LDP, 0x00,0x0b,
	PLC_SET, 0x02,0x0b,
	PLC_LDF, 0x00,0x0b,
	PLC_RST, 0x02,0x0b,
	PLC_LDP, 0x00,0x0c,
	PLC_SET, 0x02,0x0c,
	PLC_LDF, 0x00,0x0c,
	PLC_RST, 0x02,0x0c,
	PLC_LDP, 0x00,0x0d,
	PLC_SET, 0x02,0x0d,
	PLC_LDF, 0x00,0x0d,
	PLC_RST, 0x02,0x0d,
	PLC_LDP, 0x00,0x0e,
	PLC_SET, 0x02,0x0e,
	PLC_LDF, 0x00,0x0e,
	PLC_RST, 0x02,0x0e,
	PLC_LDP, 0x00,0x0f,
	PLC_SET, 0x02,0x0f,
	PLC_LDF, 0x00,0x0f,
	PLC_RST, 0x02,0x0f,
#endif
#if 1 //32路普通
	PLC_LD,  0x02,0x00,
	PLC_OUT, 0x01,0x00,
	PLC_LD,  0x02,0x01,
	PLC_OUT, 0x01,0x01,
	PLC_LD,  0x02,0x02,
	PLC_OUT, 0x01,0x02,
	PLC_LD,  0x02,0x03,
	PLC_OUT, 0x01,0x03,
	PLC_LD,  0x02,0x04,
	PLC_OUT, 0x01,0x04,
	PLC_LD,  0x02,0x05,
	PLC_OUT, 0x01,0x05,
	PLC_LD,  0x02,0x06,
	PLC_OUT, 0x01,0x06,
	PLC_LD,  0x02,0x07,
	PLC_OUT, 0x01,0x07,

	PLC_LD,  0x02,0x08,
	PLC_OUT, 0x01,0x08,
	PLC_LD,  0x02,0x09,
	PLC_OUT, 0x01,0x09,
	PLC_LD,  0x02,0x0a,
	PLC_OUT, 0x01,0x0a,
	PLC_LD,  0x02,0x0b,
	PLC_OUT, 0x01,0x0b,
	PLC_LD,  0x02,0x0c,
	PLC_OUT, 0x01,0x0c,
	PLC_LD,  0x02,0x0d,
	PLC_OUT, 0x01,0x0d,
	PLC_LD,  0x02,0x0e,
	PLC_OUT, 0x01,0x0e,
	PLC_LD,  0x02,0x0f,
	PLC_OUT, 0x01,0x0f,

	PLC_LD,  0x02,0x10,
	PLC_OUT, 0x01,0x10,
	PLC_LD,  0x02,0x11,
	PLC_OUT, 0x01,0x11,
	PLC_LD,  0x02,0x12,
	PLC_OUT, 0x01,0x12,
	PLC_LD,  0x02,0x13,
	PLC_OUT, 0x01,0x13,
	PLC_LD,  0x02,0x14,
	PLC_OUT, 0x01,0x14,
	PLC_LD,  0x02,0x15,
	PLC_OUT, 0x01,0x15,
	PLC_LD,  0x02,0x16,
	PLC_OUT, 0x01,0x16,
	PLC_LD,  0x02,0x17,
	PLC_OUT, 0x01,0x17,

	PLC_LD,  0x02,0x18,
	PLC_OUT, 0x01,0x18,
	PLC_LD,  0x02,0x19,
	PLC_OUT, 0x01,0x19,
	PLC_LD,  0x02,0x1a,
	PLC_OUT, 0x01,0x1a,
	PLC_LD,  0x02,0x1b,
	PLC_OUT, 0x01,0x1b,
	PLC_LD,  0x02,0x1c,
	PLC_OUT, 0x01,0x1c,
	PLC_LD,  0x02,0x1d,
	PLC_OUT, 0x01,0x1d,
	PLC_LD,  0x02,0x1e,
	PLC_OUT, 0x01,0x1e,
	PLC_LD,  0x02,0x1f,
	PLC_OUT, 0x01,0x1f,
#endif
	PLC_END
};

int compare_rom(const unsigned char * dat,unsigned int len)
{
	unsigned int i;
	for(i=0;i<sizeof(plc_test_flash);i++) {
		unsigned char reg;
		load_plc_form_eeprom(i,&reg,1);
		if(reg != dat[i]) {
			return 0;
		}
	}
	return 1;
}

/**********************************************
 *  获取下一条指令的指令码
 *  也许是从EEPROM中读取的程序脚本
 *  这里一次性读取下一个指令，长度为最长指令长度
 */
void plc_code_resut_to_factory(void)
{
	if(!compare_rom(plc_test_flash,sizeof(plc_test_flash))) {
	    write_plc_to_eeprom(0,plc_test_flash,sizeof(plc_test_flash));
		if(THIS_INFO)printf(" write plc to default.\r\n");
	} else {
		if(THIS_INFO)printf(" plc compare_rom successful.\r\n");
	}
}

void read_next_plc_code(void)
{
#if 1
	load_plc_form_eeprom(plc_command_index+1,plc_command_array,sizeof(plc_command_array));
#else
	memcpy(plc_command_array,&plc_test_flash[plc_command_index+1],sizeof(plc_command_array));
#endif
}

void handle_plc_command_error(void)
{
	//提示第几条指令出错
	//然后复位，或停止运行
	plc_cpu_stop = 1;
	if(THIS_ERROR)printf("\r\nhandle_plc_command_error!!!!!!\r\n");
}

unsigned char get_bitval(unsigned int index)
{
	unsigned char bitval = 0;
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		index -= IO_INPUT_BASE;
        bitval = BIT_IS_SET(inputs_new,index);
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        bitval = BIT_IS_SET(output_new,index);
    } else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        bitval = BIT_IS_SET(auxi_relays,index);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
		//unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		//B = index / 8;
		//b = index % 8;
		//holder_register_read(B,&reg,1);
		bitval = BIT_IS_SET(hold_register_map,index);
	} else if(index >= SPECIAL_RELAY_BASE && index < (SPECIAL_RELAY_BASE+SPECIAL_RELAY_COUNT)) {
		//读取上电保持继电器状态，人工清零 
		index -= SPECIAL_RELAY_BASE;
		bitval = BIT_IS_SET(speicial_relays,index);
	} else if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
		index -= TIMING100MS_EVENT_BASE;
        bitval = BIT_IS_SET(tim100ms_arrys.event_bits,index);
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE + TIMING1S_EVENT_COUNT)) {
		index -= TIMING1S_EVENT_BASE;
        bitval = BIT_IS_SET(tim1s_arrys.event_bits,index);
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
        bitval = BIT_IS_SET(counter_arrys.event_bits,index);
	} else {
		//handle_plc_command_error();
		bitval = 0;
	}
	return bitval;
}

static unsigned char get_last_bitval(unsigned int index)
{
	unsigned char bitval = 0;
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		index -= IO_INPUT_BASE;
        bitval = BIT_IS_SET(inputs_last,index);
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        bitval = BIT_IS_SET(output_last,index);
	} else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        bitval = BIT_IS_SET(auxi_relays_last,index);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
		//unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		//index += AUXI_HOLDRELAY_COUNT / 8; //后面一部分是上一次的
		//B = index / 8;
		//b = index % 8;
		//holder_register_read(B,&reg,1);
		//bitval = BIT_IS_SET(&reg,b);
		bitval = BIT_IS_SET(hold_register_map_last,index);
	} else if(index >= SPECIAL_RELAY_BASE && index < (SPECIAL_RELAY_BASE+SPECIAL_RELAY_COUNT)) {
		//读取上电保持继电器状态，人工清零 
		index -= SPECIAL_RELAY_BASE;
		bitval = BIT_IS_SET(speicial_relays_last,index);
	} else if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
		index -= TIMING100MS_EVENT_BASE;
        bitval = BIT_IS_SET(tim100ms_arrys.event_bits_last,index);
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE + TIMING1S_EVENT_COUNT)) {
		index -= TIMING1S_EVENT_BASE;
        bitval = BIT_IS_SET(tim1s_arrys.event_bits_last,index);
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
        bitval = BIT_IS_SET(counter_arrys.event_bits_last,index);
	} else {
		//handle_plc_command_error();
		bitval = 0;
	}
	return bitval;
}

void set_bitval(unsigned int index,unsigned char bitval)
{
	if(index >= IO_INPUT_BASE && index < (IO_INPUT_BASE+IO_INPUT_COUNT)) {
		//输入值不能修改
	} else if(index >= IO_OUTPUT_BASE && index < (IO_OUTPUT_BASE+IO_OUTPUT_COUNT)) {
		index -= IO_OUTPUT_BASE;
        SET_BIT(output_new,index,bitval);
	} else if(index >= AUXI_RELAY_BASE && index < (AUXI_RELAY_BASE + AUXI_RELAY_COUNT)) {
		index -= AUXI_RELAY_BASE;
        SET_BIT(auxi_relays,index,bitval);
    } else if(index >= AUXI_HOLDRELAY_BASE && index < (AUXI_HOLDRELAY_BASE + AUXI_HOLDRELAY_COUNT)) {
		//unsigned char B,b,reg;
		index -= AUXI_HOLDRELAY_BASE;
		//B = index / 8;
		//b = index % 8;
		//holder_register_read(B,&reg,1);
		//SET_BIT(&reg,b,bitval);
		//holder_register_write(B,&reg,1);
		SET_BIT(hold_register_map,index,bitval);
		hold_durty = 1;
	} else if(index >= SPECIAL_RELAY_BASE && index < (SPECIAL_RELAY_BASE+SPECIAL_RELAY_COUNT)) {
		//读取上电保持继电器状态，人工清零 
		index -= SPECIAL_RELAY_BASE;
		SET_BIT(speicial_relays,index,bitval);
	} else if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    //计数器的值不可以置位,只可以复位
		if(!bitval) {
		    index -= COUNTER_EVENT_BASE;
            counter_arrys.counter[index] = 0;
            SET_BIT(counter_arrys.event_bits,index,0);
		}
	} else {
		//handle_plc_command_error();
	}
}

unsigned char get_byte_val(unsigned int index)
{
	unsigned char reg;
	if(index >= REG_BASE && index < (REG_BASE+REG_COUNT)) {
		//普通字节变量
		reg = general_reg[index-REG_BASE];
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//读取RTC时间,地址分别是年月日，时分秒，星期
		reg = rtc_reg[index-REG_RTC_BASE];
	} else if(index >=REG_TEMP_BASE && index <(REG_TEMP_BASE+REG_TEMP_COUNT)) {
		reg = temp_reg[index-REG_TEMP_BASE];
	} else {
		reg = 0;
	}
	return reg;
}

void set_byte_val(unsigned int index,unsigned char val)
{
	if(index >= REG_BASE && index < (REG_BASE+REG_COUNT)) {
		//普通字节变量
		general_reg[index-REG_BASE] = val;
	} else if(index >=REG_RTC_BASE && index <(REG_RTC_BASE+REG_RTC_COUNT)) {
		//读取RTC时间,地址分别是年月日，时分秒，星期
		//rtc_reg[index-REG_RTC_BASE] = val;暂时不能修改时间
	} else if(index >=REG_TEMP_BASE && index <(REG_TEMP_BASE+REG_TEMP_COUNT)) {
	}
}

/**********************************************
 * 根据条件对计时器进行增加
 * 如果时间到了，则触发事件
 * 如果时间尚未到，则继续计时
 */
void timing_cell_prcess(void)
{
	unsigned int  i;
	unsigned int  counter;
	sys_lock();
	counter = time100ms_come_flag;
	time100ms_come_flag = 0;
	sys_unlock();
    {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
	    for(i=0;i<TIMING100MS_EVENT_COUNT;i++) {
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //如果允许计时
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //如果时间事件未发生
				    if(ptiming->counter[i] > counter) {
					    ptiming->counter[i] -= counter;
					} else {
					    ptiming->counter[i] = 0;
					}
					if(ptiming->counter[i] == 0) {
					    SET_BIT(ptiming->event_bits,i,1);
						//if(THIS_INFO)putrsUART((ROM char*)"timing100ms event come.");
					}
				}
			} else {
			    if(BIT_IS_SET(ptiming->holding_bits,i)) {
				    //保持定时器
				} else {
				    //不保持
					ptiming->counter[i] = 0;
					SET_BIT(ptiming->event_bits,i,0);
					//if(THIS_INFO)putrsUART((ROM char*)"timing100ms end.");
				}
			}
	    }
	}
 	sys_lock();
	counter = time1s_come_flag;
	time1s_come_flag = 0;
	sys_unlock();
	{
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
	    for(i=0;i<TIMING1S_EVENT_COUNT;i++) {
		    if(BIT_IS_SET(ptiming->enable_bits,i)) { //如果允许计时
			    if(counter > 0 && !BIT_IS_SET(ptiming->event_bits,i)) {  //如果时间事件未发生
				    if(ptiming->counter[i] > counter) {
					    ptiming->counter[i] -= counter;
					} else {
					    ptiming->counter[i] = 0;
						//if(THIS_INFO)putrsUART((ROM char*)"1s come.");
					}
					if(ptiming->counter[i] == 0) {
					    SET_BIT(ptiming->event_bits,i,1);
						//if(THIS_INFO)putrsUART((ROM char*)"timing1s event [come].");
					}
				}
			} else {
			    if(BIT_IS_SET(ptiming->holding_bits,i)) {
				    //保持定时器
				} else {
				    //不保持
					ptiming->counter[i] = 0;
					SET_BIT(ptiming->event_bits,i,0);
					//if(THIS_INFO)putrsUART((ROM char*)"timing1s end.");
				}
			}
	    }
	}
}

/**********************************************
 * 打开定时器，并设定触发时间的最大值
 * 如果已经开始计时，则继续计时，如果没有开始，则开始
 */
static void timing_cell_start(unsigned int index,unsigned int event_count,unsigned char upordown,unsigned char holding)
{
	if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
		index -= TIMING100MS_EVENT_BASE;
		if(!BIT_IS_SET(ptiming->enable_bits,index)) {
		    ptiming->counter[index]    = event_count;
			SET_BIT(ptiming->enable_bits,  index,1);
			SET_BIT(ptiming->upordown_bits,index,upordown);
			SET_BIT(ptiming->holding_bits, index,holding);
			SET_BIT(ptiming->event_bits,   index,0);
			//if(THIS_INFO)putrsUART((ROM char*)"timing100ms start.");
		}
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE+TIMING1S_EVENT_COUNT)) {
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
		index -= TIMING1S_EVENT_BASE;
		if(!BIT_IS_SET(ptiming->enable_bits,index)) {
		    ptiming->counter[index]    = event_count;
			SET_BIT(ptiming->enable_bits,  index,1);
			SET_BIT(ptiming->upordown_bits,index,upordown);
			SET_BIT(ptiming->holding_bits, index,holding);
			SET_BIT(ptiming->event_bits,   index,0);
			//if(THIS_INFO)putrsUART((ROM char*)"timing1s start.");
		}
	} else {
		handle_plc_command_error();
	}
}

/**********************************************
 * 关闭定时器，并取消触发事件
 */
static void timing_cell_stop(unsigned int index)
{
	if(index >= TIMING100MS_EVENT_BASE && index < (TIMING100MS_EVENT_BASE+TIMING100MS_EVENT_COUNT)) {
	    TIM100MS_ARRAYS_T * ptiming = &tim100ms_arrys;
		index -= TIMING100MS_EVENT_BASE;
		if(BIT_IS_SET(ptiming->enable_bits,index)) {
		    SET_BIT(ptiming->enable_bits,  index,0);
		}
	} else if(index >= TIMING1S_EVENT_BASE && index < (TIMING1S_EVENT_BASE+TIMING1S_EVENT_COUNT)) {
	    TIM1S_ARRAYS_T * ptiming = &tim1s_arrys;
		index -= TIMING1S_EVENT_BASE;
		if(BIT_IS_SET(ptiming->enable_bits,index)) {
		    //if(THIS_INFO)putrsUART((ROM char*)"timing1ms stop.");
		    SET_BIT(ptiming->enable_bits,  index,0);
		}
	} else {
		//if(THIS_ERROR)putrsUART((ROM char*)"timing stop error");
	    handle_plc_command_error();
	}
}

/**********************************************
 * 加载输入端口的输入值
 */
void handle_plc_ld(void)
{
	bit_acc = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_LDI) {
		bit_acc = !bit_acc;
	}
	plc_command_index += 3;
}
/**********************************************
 * 把位运算的结果输出到输出端口中
 */
void handle_plc_out(void)
{
	set_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]),bit_acc);
	plc_command_index += 3;
}


/**********************************************
 * 与或与非运算
 */
void handle_plc_and_ani(void)
{
	unsigned char bittmp = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_AND) {
	    bit_acc = bit_acc && bittmp;
	} else {
		bit_acc = bit_acc && (!bittmp);
	}
	plc_command_index += 3;
}

/**********************************************
 * 或或或非运算
 */
void handle_plc_or_ori(void)
{
	unsigned char  bittmp = get_bitval(HSB_BYTES_TO_WORD(&plc_command_array[1]));
	if(PLC_CODE == PLC_OR) {
	    bit_acc = bit_acc || bittmp;
	} else if(PLC_CODE == PLC_ORI) {
		bit_acc = bit_acc || (!bittmp);
	}
	plc_command_index += 3;
}

/**********************************************
 * 加载输入端上升沿或下降沿
 */
void handle_plc_ldp_ldf(void)
{
	unsigned char  reg;
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg  = get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_LDP) {
		if(reg == 0x02) { //只管上升沿，其他情况都是假的
			bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	} else if(PLC_CODE == PLC_LDF) {
		if(reg == 0x01) {//只管下降沿，其他情况都是假的
		    bit_acc = 1;
		} else {
		    bit_acc = 0;
		}
	}
	plc_command_index += 3;
}


/**********************************************
 * 与上升沿或下降沿
 */
void handle_plc_andp_andf(void)
{
    unsigned char  reg;
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg =  get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_ANDP) {
		if(reg == 0x02) {
			bit_acc = bit_acc && 1;
		} else {
			bit_acc = 0;
		}
	} else if(PLC_CODE == PLC_ANDF) {
		if(reg == 0x01) {
		    bit_acc = bit_acc && 1;
		} else {
			bit_acc = 0;
		}
	}
	plc_command_index += 3;
}
/**********************************************
 * 或上升沿或下降沿
 */
void handle_plc_orp_orf(void)
{
	unsigned char  reg;
	unsigned int   bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	reg =  get_last_bitval(bit_index)?0x01:0x00;
	reg |= get_bitval(bit_index)?     0x02:0x00;
	if(PLC_CODE == PLC_ORP) {
		if(reg == 0x02) {
			bit_acc = bit_acc || 1;
		} else {
			bit_acc = bit_acc || 0;
		}
	} else if(PLC_CODE == PLC_ORF) {
		if(reg == 0x01) {
		    bit_acc = bit_acc || 1;
		} else {
			bit_acc = bit_acc || 0;
		}
	}
	plc_command_index += 3;
}




/**********************************************
 * 压栈、出栈、读栈
 */
void handle_plc_mps_mrd_mpp(void)
{
	unsigned char  B,b;
	if(PLC_CODE == PLC_MPS) {
		if(bit_stack_sp >= BIT_STACK_LEVEL) {
		    if(THIS_ERROR)printf("堆栈溢出\r\n");
		    handle_plc_command_error();
			return ;
		}
	    B = bit_stack_sp / 8;
	    b = bit_stack_sp % 8;
		if(bit_acc) {
			bit_stack[B] |=  code_msk[b];
		} else {
			bit_stack[B] &= ~code_msk[b];
		}
		bit_stack_sp++;
	} else if(PLC_CODE == PLC_MRD) {
	    unsigned char last_sp = bit_stack_sp - 1;
	    B = last_sp / 8;
	    b = last_sp % 8;
		bit_acc = (bit_stack[B] & code_msk[b])?1:0;
	} else if(PLC_CODE == PLC_MPP) {
	    bit_stack_sp--;
	    B = bit_stack_sp / 8;
	    b = bit_stack_sp % 8;
		bit_acc = (bit_stack[B] & code_msk[b])?1:0;
	}
	plc_command_index += 1;
}
/**********************************************
 * 锁或解锁指令
 */
void handle_plc_set_rst(void)
{
	unsigned int bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(PLC_CODE == PLC_SET) {
	    if(bit_acc) {
			set_bitval(bit_index,1);
	    }
	} else if(PLC_CODE == PLC_RST) {
	    if(bit_acc) {
			set_bitval(bit_index,0);
	    }
	}
	plc_command_index += 3;
}
/**********************************************
 * 有输入去翻，没输入，不改变
 */
void handle_plc_seti(void)
{
	unsigned int  bit_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(bit_acc) {
        set_bitval(bit_index,!get_bitval(bit_index));
	}
	plc_command_index += 3;
}
/**********************************************
 * 取反结果
 */
void handle_plc_inv(void)
{
	bit_acc = !bit_acc;
	plc_command_index += 1;
}

/**********************************************
 * 输出到定时器
 * 根据编号，可能输出到100ms定时器，也可能输出到1s定时器
 */
void handle_plc_out_t(void)
{
	unsigned int  kval;
	unsigned int  time_index = HSB_BYTES_TO_WORD(&plc_command_array[1]);
	if(bit_acc) {
	    kval = HSB_BYTES_TO_WORD(&plc_command_array[3]);
	    timing_cell_start(time_index,kval,1,0);
	} else {
		timing_cell_stop(time_index);
	}
	plc_command_index += 5;
}
/**********************************************
 * 输出到计数器
 */
void handle_plc_out_c(void)
{
	unsigned int  kval = HSB_BYTES_TO_WORD(&plc_command_array[3]);
	unsigned int  index = HSB_BYTES_TO_WORD(&plc_command_array[1]);

	//判断索引是否有效
    if(index >= COUNTER_EVENT_BASE && index < (COUNTER_EVENT_BASE+COUNTER_EVENT_COUNT)) {
	    index -= COUNTER_EVENT_BASE;
	} else {
	   // if(THIS_ERROR)printf("输出计数器索引值有错误!\r\n");
		handle_plc_command_error();
	    return ;
	}
	//计数器内部维持上一次的触发电平
	//触发计数器的时候，把这次的触发电平触发进计数器
	if(bit_acc) {
	    if(index < COUNTER_EVENT_COUNT) {
	        if(!BIT_IS_SET(counter_arrys.last_trig_bits,index)) {
		        //上一次是低电平，这次是高电平，可以触发计数
			    if(counter_arrys.counter[index] < kval) {
			        if(++counter_arrys.counter[index] == kval) {
				        SET_BIT(counter_arrys.event_bits,index,1);
				    }
			    }
		    }
		}
	}
	//把电平记录到计数器去
	if(index < COUNTER_EVENT_COUNT) {
	    SET_BIT(counter_arrys.last_trig_bits,index,bit_acc);
	}
	plc_command_index += 5;
}

/**********************************************
 * 区间复位指令，比如把Y0 - Y7 复位
 * 第一版end不闭合，就是开合
 * 这个不行，应该是闭合的，即包括最后一个
 */
void handle_plc_zrst(void)
{
	typedef struct _zrst_command
	{
		unsigned char cmd;
		unsigned char start_hi;
		unsigned char start_lo;
		unsigned char end_hi;
		unsigned char end_lo;
	} zrst_command;
	zrst_command * plc = (zrst_command *)plc_command_array;
	unsigned int start = HSB_BYTES_TO_WORD(&plc->start_hi);
	unsigned int end = HSB_BYTES_TO_WORD(&plc->end_hi);
	unsigned int i;
	for(i=start;i<=end;i++) {
		set_bitval(i,0);
	}
	plc_command_index += sizeof(zrst_command);
}
/**********************************************
 * 比较指令
 * BCMP   K   B   M0
 * 第一版发货的: K < reg 1,0,0, K = reg ==> 0,1,0    K > reg ==> 0,0,1
 * 经过测试，指令不够直观，应该是变化的寄存器小于K，应该是1,0,0,
 * 第二版改为： reg < K 1,0,0, K = reg ==> 0,1,0    reg > K ==> 0,0,1
 */
void handle_plc_bcmp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char kval;
		unsigned char reg_hi;
		unsigned char reg_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_command;
	plc_command * plc = (plc_command *)plc_command_array;
	unsigned char reg = get_byte_val(HSB_BYTES_TO_WORD(&plc->reg_hi));
	unsigned int  out = HSB_BYTES_TO_WORD(&plc->out_hi);
	if(plc->cmd == PLC_BCMP) {  //三个寄存器的指令
	    if(reg < plc->kval) {
		    set_bitval(out,1);
		    set_bitval(out+1,0);
		    set_bitval(out+2,0);
	    } else if(reg == plc->kval) {
		    set_bitval(out,0);
		    set_bitval(out+1,1);
		    set_bitval(out+2,0);
	    } else if(reg > plc->kval) {
		    set_bitval(out,0);
		    set_bitval(out+1,0);
		    set_bitval(out+2,1);
	    }
	} else if(plc->cmd == PLC_BCMPL) {
	    if(reg < plc->kval) {
		    set_bitval(out,1);
			bit_acc = 1;
		} else {
		    set_bitval(out,0);
			bit_acc = 0;
	    }
	} else if(plc->cmd == PLC_BCMPB) {
	    if(reg > plc->kval) {
		    set_bitval(out,1);
			bit_acc = 1;
		} else {
		    set_bitval(out,0);
			bit_acc = 0;
	    }
	} else if(plc->cmd == PLC_BCMPE) {  //单个寄存器的指令
	    if(reg == plc->kval) {
		    set_bitval(out,1);
			bit_acc = 1;
		} else {
		    set_bitval(out,0);
			bit_acc = 0;
	    }
	}
	plc_command_index += sizeof(plc_command);
}

/**********************************************
 * 多字节比较指令
 * 注意，是寄存器的值，跟常数比较，BACMPL是寄存器小于的意思，以此类推
 * 左边即是常数，右边是寄存器
 */
void handle_plc_bacmp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char size;
		unsigned char k_base;  //后面还有很多k，总共有size个
	} plc_command;
	typedef struct _plc_bacmp_t
	{
		unsigned char b_base_hi;
		unsigned char b_base_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_bacmp_t;
	plc_command  * plc  = (plc_command *)plc_command_array;
	plc_bacmp_t * pacmp = (plc_bacmp_t *)(plc_command_array+sizeof(plc_command) - 1 +plc->size);  //减去k_base占位一字节
	unsigned char * pk  = &plc->k_base;
	unsigned int base   = HSB_BYTES_TO_WORD(&pacmp->b_base_hi);
	unsigned int out    = HSB_BYTES_TO_WORD(&pacmp->out_hi);
	unsigned char result = 0;
	//从右边比起
	if(plc->cmd == PLC_BACMPB) {
		unsigned char i;
		result = 0;
	    for(i=plc->size;i>0;i--) {
		    unsigned char k = pk[i-1];
		    unsigned char reg = get_byte_val(base+i-1);
			if(reg > k) {
				result = 1;
				break;
			} else if(reg < k) {
				break;
			}
	    }
	} else if(plc->cmd == PLC_BACMPE) {
		unsigned char i;
		result = 1;
	    for(i=plc->size;i>0;i--) {
		    unsigned char lreg = pk[i-1];
		    unsigned char rreg = get_byte_val(base+i-1);
			if(lreg != rreg) { //发现假的条件，立刻退出
				result = 0;
				break;
			}
	    }
	} else if(plc->cmd == PLC_BACMPL) {
		unsigned char i;
		result = 0;
	    for(i=plc->size;i>0;i--) {
		    unsigned char k = pk[i-1];
		    unsigned char reg = get_byte_val(base+i-1);
			if(reg < k) {
				result = 1;
				break;
			} else if(reg > k) {
				break;
			}
	    }
	}
	set_bitval(out,result);
	bit_acc = result;
	plc_command_index += sizeof(plc_command) + sizeof(plc_bacmp_t) + plc->size - 1; //减去k_base占位一字节
}

/**********************************************
 * 多字节区间比较指令
 * 注意，左边几个 ，右边几个，中间是寄存器
 */
void handle_plc_bazcp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char size;
		unsigned char k_base;  //后面还有很多k，总共有size个
	} plc_command;
	typedef struct _plc_bacmp_t
	{
		unsigned char b_base_hi;
		unsigned char b_base_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_bacmp_t;
	plc_command  * plc  = (plc_command *)plc_command_array;
	plc_bacmp_t * pacmp = (plc_bacmp_t *)(plc_command_array+sizeof(plc_command) - 1 + plc->size * 2);  //减去k_base占位一字节
	unsigned char * pkl  = &plc->k_base;
	unsigned char * pkr  = pkl + plc->size;
	unsigned int base   = HSB_BYTES_TO_WORD(&pacmp->b_base_hi);
	unsigned int out    = HSB_BYTES_TO_WORD(&pacmp->out_hi);
	unsigned char result;
	//先比较左边小于条件
	unsigned char i;
	result = 0; //假设在左边
	for(i=plc->size;i>0;i--) {
	    unsigned char k = pkl[i-1];
	    unsigned char reg = get_byte_val(base+i-1);
		if(reg < k) {
			break;
		} else if(reg > k) { //判断结果在右边
			result = 1;
			break;
		}
	}
	if(result == 1) { //左边成立的
	    for(i=plc->size;i>0;i--) {
		    unsigned char k = pkr[i-1];
		    unsigned char reg = get_byte_val(base+i-1);
			if(reg > k) {  //判断结果在最右边
				result = 2;
				break;
			} else if(reg < k) {
				break;
			}
	    }
	}
	if(result == 0) {
		set_bitval(out,1);
		set_bitval(out+1,0);
		set_bitval(out+2,0);
	} else if(result == 1) {
		set_bitval(out,0);
		set_bitval(out+1,1);
		set_bitval(out+2,0);
	} else if(result == 2) {
		set_bitval(out,0);
		set_bitval(out+1,0);
		set_bitval(out+2,1);
	}
	plc_command_index += sizeof(plc_command) + sizeof(plc_bacmp_t) + plc->size*2 - 1; //减去k_base占位一字节
}


/**********************************************
 * 字节区间比较指令
 * BZCP   K1  K2  B  M0
 * 该指令为字节比较指令，将比较的结果<,中间,>三种结果分别告知给M0，M1，M2。
 */
void handle_plc_bzcp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char klow;
		unsigned char khig;
		unsigned char reg_hi;
		unsigned char reg_lo;
		unsigned char out_hi;
		unsigned char out_lo;
	} plc_command;
	plc_command * plc = (plc_command *)plc_command_array;
	unsigned char reg = get_byte_val(HSB_BYTES_TO_WORD(&plc->reg_hi));
	unsigned int  out = HSB_BYTES_TO_WORD(&plc->out_hi);
	if(plc->cmd == PLC_BZCP) {
	    if(reg < plc->klow) {
		    set_bitval(out,1);
		    set_bitval(out+1,0);
		    set_bitval(out+2,0);
	    } else if(reg >= plc->klow && reg <= plc->khig) {
		    set_bitval(out,0);
		    set_bitval(out+1,1);
		    set_bitval(out+2,0);
	    } else if(reg > plc->khig) {
		    set_bitval(out,0);
		    set_bitval(out+1,0);
		    set_bitval(out+2,1);
	    }
	} else if(plc->cmd == PLC_BZCPS) {
		if(reg >= plc->klow && reg <= plc->khig) {
			set_bitval(out,1);
			bit_acc = 1;
		} else {
			set_bitval(out,0);
			bit_acc = 0;
		}
	}
	plc_command_index += sizeof(plc_command);
}

/**********************************************
 * 跳转和条件跳转指令
 */
void handle_plc_jmp(void)
{
	typedef struct _plc_command
	{
		unsigned char cmd;
		unsigned char jmp_step_hi;
		unsigned char jmp_step_lo;
	} plc_command;
	plc_command * plc = (plc_command *)plc_command_array;
	unsigned int step = HSB_BYTES_TO_WORD(&plc->jmp_step_hi);
	if(plc->cmd == PLC_JMPS) {
		if(bit_acc) {
		    plc_command_index += step;
		}
	} else if(plc->cmd == PLC_JMP) {
	    plc_command_index += step;
	}
	plc_command_index += sizeof(plc_command);
}







/**********************************************
 * 通信位指令处理程序
 * 如果找到接收到这个指令，则处理它，然后返回一个数据
 * 这个指令可以一定允许，和激活，程序将等待上位机的位写指令
 * 并且程序也定期询问指定的远程主机ID，非此ID，不接受
 * 此ID主机可能是通用任意主机，广播主机，或指定IP的主机
 */
//网络读指令


void handle_plc_net_rb(void)
{
  //定义通信指令
  typedef struct _NetRdOptT
  {
    unsigned char op;
    unsigned char net_index;  //发送指令索引，因为发送需要间隔轮询，不能一起发送，这样会造成内存紧张
    unsigned char remote_device_addr;
    unsigned char remote_start_addr_hi;  //远端数据的起始地址
    unsigned char remote_start_addr_lo;  //远端数据的起始地址
    unsigned char local_start_addr_hi;  //远端数据的起始地址
    unsigned char local_start_addr_lo;  //远端数据的起始地址
    unsigned char data_number; //通信数据的个数
    //输入变量
    unsigned char enable_addr_hi;
    unsigned char enable_addr_lo;
    //激活一次通信
    unsigned char request_addr_hi;
    unsigned char request_addr_lo;
    //通信进行中标记
    unsigned char txing_hi;
    unsigned char txing_lo;
    //完成地址
    unsigned char done_addr_hi;
    unsigned char done_addr_lo;
    //超时定时器索引
    unsigned char timeout_addr_hi;
    unsigned char timeout_addr_lo;
    //定时超时,S
    unsigned char timeout_val;
  } NetRdOptT;
  //
#if 0
  NetRdOptT * p = (NetRdOptT *)plc_command_array;
  DATA_RX_PACKET_T * prx;
  //轮到这个通信指令执行时间了
  if(net_global_send_index == p->net_index) {  //拿到令牌的
	  rx_look_up_packet(); //拿到令牌的人，必须看一遍接收的数据，要不要要说一声。
      //是的，可以发送
	  if(get_bitval(HSB_BYTES_TO_WORD(&p->enable_addr_hi))) { //这个通信单元被使能的
        if(get_bitval(HSB_BYTES_TO_WORD(&p->txing_hi))) {
            //正在发送中，判断是否超时
            if(get_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi))) {
                //超时了，那么，重启一次发送程序
                set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
                goto try_again;
            } else {
                //没有超时，那么等待应答数据是否到了
                unsigned int i;
                for(i=0;i<RX_PACKS_MAX_NUM;i++) {
                    prx = &rx_ctl.rx_packs[i];
                    if(prx->finished) {
                        //MODBUS位指令翻译，根据翻译结果设置指定的位置
                        unsigned int localbits = HSB_BYTES_TO_WORD(&p->local_start_addr_hi);
                        if(THIS_ERROR)printf("rb get one rx packet.");
						if(1) { ///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!modbus_prase_read_multi_coils_ack(p->remote_device_addr,prx->buffer,prx->index,localbits,p->data_number)) {
                             //应答数据OK，可以完成此次数据请求，等待下一循环的请求
                            if(THIS_ERROR)printf("rb rx ack data is ok.");
                            set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),0);
                            set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),1);
                            break; 
		    		    }
                    }
    			}
                if(prx == NULL) {
                    //没有收到应答耶，那我们再等一等吧。。。
                    //if(THIS_ERROR)printf("rb timeout,resend packet.");
    			}
	    	}
		} else {
            //尚未发送
try_again:
    	 	if(get_bitval(HSB_BYTES_TO_WORD(&p->request_addr_hi))) {
                if(THIS_ERROR)printf("rb read coils send request.");
		    	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1modbus_read_multi_coils_request(HSB_BYTES_TO_WORD(&p->local_start_addr_hi),p->data_number,p->remote_device_addr);
                //然后启动定时器
                timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
                timing_cell_start(HSB_BYTES_TO_WORD(&p->timeout_addr_hi),p->timeout_val,1,0);
                //置启动标记
                set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),1);
	    	} else {
			    //允许发送，还没需要发送呢
			}
		}
	  } else {
	    //发送使能被关闭的
        //停止定时器
        timing_cell_stop(HSB_BYTES_TO_WORD(&p->timeout_addr_hi));
        set_bitval(HSB_BYTES_TO_WORD(&p->txing_hi),0);
        set_bitval(HSB_BYTES_TO_WORD(&p->done_addr_hi),0);
	  }
  } else {
	  //没有拿到令牌，那就等下一次吧
  }
#endif
  plc_command_index += sizeof(NetRdOptT);
}


void handle_plc_net_wb(void)
{
}


void PlcProcess(void)
{
	//输入处理,读取IO口的输入
	phy_io_in_get_bits(0,inputs_new,IO_INPUT_COUNT);
	//处理通信程序
	//初始化通信令牌
    net_communication_count = 0; ////!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!plc_test_buffer[0]; //打个比喻，代码里面存在5处发送
	plc_command_index = 0;
	plc_cpu_stop = 0;
 next_plc_command:
	read_next_plc_code();
	//逻辑运算,调用一次，运行一次用户的程序
	switch(PLC_CODE)
	{
	case PLC_END: //指令结束，处理后事
		plc_command_index = 0;
		goto plc_command_finished;
	case PLC_LD: 
	case PLC_LDI:
		handle_plc_ld();
		break;
    case PLC_LDKH:
        bit_acc = 1;
        plc_command_index++;
        break;
    case PLC_LDKL:
        bit_acc = 0;
        plc_command_index++;
        break;
    case PLC_SEI:
        handle_plc_seti();
        break;
	case PLC_OUT:
		handle_plc_out();
		break;
	case PLC_AND:
	case PLC_ANI:
		handle_plc_and_ani();
		break;
	case PLC_OR:
	case PLC_ORI:
		handle_plc_or_ori();
		break;
	case PLC_LDP:
	case PLC_LDF:
		handle_plc_ldp_ldf();
		break;
	case PLC_ANDP:
	case PLC_ANDF:
		handle_plc_andp_andf();
		break;
	case PLC_ORP:
	case PLC_ORF:
		handle_plc_orp_orf();
		break;
	case PLC_MPS:
	case PLC_MRD:
	case PLC_MPP:
		handle_plc_mps_mrd_mpp();
		break;
	case PLC_SET:
	case PLC_RST:
		handle_plc_set_rst();
		break;
	case PLC_INV:
		handle_plc_inv();
		break;
	case PLC_OUTT:
		handle_plc_out_t();
		break;
	case PLC_OUTC:
		handle_plc_out_c();
		break;
	case PLC_ZRST:
		handle_plc_zrst();
		break;
    case PLC_NETRB:
        handle_plc_net_rb(); 
        break;
	case PLC_BCMP:
	case PLC_BCMPE:
	case PLC_BCMPL:
	case PLC_BCMPB:
		handle_plc_bcmp();
		break;
	case PLC_BACMPL:
	case PLC_BACMPE:
	case PLC_BACMPB:
		handle_plc_bacmp();
		break;
	case PLC_BZCP:
	case PLC_BZCPS:
		handle_plc_bzcp();
		break;
	case PLC_BAZCP:
		handle_plc_bazcp();
		break;
	case PLC_JMP:
	case PLC_JMPS:
		handle_plc_jmp();
		break;
    case PLC_NETWB:
    case PLC_NETRW:
    case PLC_NETWW:
	default:
	    handle_plc_command_error();
		goto plc_command_finished;
	case PLC_NONE: //空指令，直接跳过
        plc_command_index++;
		break;
	}
	if(plc_cpu_stop) {
		goto plc_command_finished;
	}
	goto next_plc_command;
 plc_command_finished:
	//输出处理，把运算结果输出到继电器中
	phy_io_out_set_bits(0,output_new,IO_OUTPUT_COUNT);
	memcpy(output_last,output_new,sizeof(output_new));
	//后续处理
	memcpy(inputs_last,inputs_new,sizeof(inputs_new));
	//辅助继电器
	memcpy(auxi_relays_last,auxi_relays,sizeof(auxi_relays));
	//特殊继电器
	memcpy(speicial_relays_last,speicial_relays,sizeof(speicial_relays));
	{ //写到RTC芯片中
		if(hold_durty) { //脏了，必须写回去
		    holder_register_write(0,hold_register_map,sizeof(hold_register_map));
			hold_durty = 0;
		}
		//保持继电器的内存搬移
		memcpy(hold_register_map_last,hold_register_map,sizeof(hold_register_map));
	}
	//系统时间处理，可以拿到定时器中断处理
	memcpy(tim100ms_arrys.event_bits_last,tim100ms_arrys.event_bits,sizeof(tim100ms_arrys.event_bits));
	//
	memcpy(tim1s_arrys.event_bits_last,tim1s_arrys.event_bits,sizeof(tim1s_arrys.event_bits));
	//
	memcpy(counter_arrys.event_bits_last,counter_arrys.event_bits,sizeof(counter_arrys.event_bits));
	//定时器处理
	timing_cell_prcess();
	//计数器处理
    //把接收到的无用的数据清理掉
    //rx_free_useless_packet(net_communication_count); !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if(++net_global_send_index >= net_communication_count) {
        //令牌溢出，重新来一遍，每个人都有机会做一次通信动作
        net_global_send_index = 0;
    }
}



THREAD(plc_rtthread, arg)
{
	NutThreadSetPriority(RT_THREAD_PRI);
    while(1)
	{
		NutSleep(90);
		plc_timing_tick_process();
	}
}

THREAD(plc_thread, arg)
{
	if(THIS_INFO)printf("plc_thread run...\r\n");
	NutThreadSetPriority(PLC_PRIORITY_LEVEL);
	PlcInit();
    while(1)
	{
		PlcProcess();
		NutSleep(50);
		if(secend_tick_come) {
			secend_tick_come = 0;
		    plc_rtc_tick_process();
		}
	}
}

void StartPlcThread(void)
{
	NutThreadCreate("plc_thread",  plc_thread, 0, 1024);
	NutThreadCreate("plc_rtthread",  plc_rtthread, 0, 64);
}
