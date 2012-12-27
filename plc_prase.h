#ifndef __PLC_PRASE_H__
#define __PLC_PRASE_H__

//--------------------------地址编号-------数量-----------
#define  IO_INPUT_BASE          0
#define  IO_INPUT_COUNT                     INPUT_CHANNEL_NUM
#define  IO_OUTPUT_BASE         256  //0x01,0x00
#define  IO_OUTPUT_COUNT                    OUTPUT_CHANNEL_NUM
#define  AUXI_RELAY_BASE        512  //0x02,0x00
#define  AUXI_RELAY_COUNT                   100
#define  AUXI_HOLDRELAY_BASE    1024 //0x04,0x00
#define  AUXI_HOLDRELAY_COUNT               (HOLDER_REGISTER_BYTES*8)

#define  SPECIAL_RELAY_BASE     1536 //0x06,0x00
#define  SPECIAL_RELAY_COUNT                1   //只有一位，复位标记继电器
//
#define  TIMING100MS_EVENT_BASE  2048  //0x08,0x00
#define  TIMING100MS_EVENT_COUNT            24

#define  TIMING1S_EVENT_BASE     3072  //0x0C,0x00
#define  TIMING1S_EVENT_COUNT               24

#define  COUNTER_EVENT_BASE      4096  //0x10,0x00
#define  COUNTER_EVENT_COUNT                24
//字节变量
#define  REG_BASE                0
#define  REG_COUNT                          128
//时间地址
#define  REG_RTC_BASE            0x2000  //8192
#define  REG_RTC_COUNT                      7    //年月日，时分秒，星期
#define  REG_TEMP_BASE           0x2007  //8199
#define  REG_TEMP_COUNT                     2    //温度高地位

//http://192.168.1.223/mpfsupload

extern void PlcInit(void);
extern void PlcProcess(void);
extern void plc_timing_tick_process(void);
extern unsigned char plc_write_delay(void);
extern void plc_set_busy(unsigned char busy);
extern void set_bitval(unsigned int index,unsigned char bitval);
extern unsigned char get_bitval(unsigned int index);

extern void plc_code_test_init(void);

#define sys_lock()   NutEnterCritical()
#define sys_unlock() NutExitCritical()


extern void StartPlcThread(void);


#endif