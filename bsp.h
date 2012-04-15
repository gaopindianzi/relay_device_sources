#ifndef __NUT_BSP_H__
#define __NUT_BSP_H__


#if 1 //放到Makefile中定义
#define    EXT_BOARD_IS_6CHIN_7CHOUT        1
#define    EXT_BOARD_IS_8CHIN_8CHOUT        2
#define    EXT_BOARD_IS_CAN485_MINIBOARD    3
#define    EXT_BOARD_IS_16CHOUT             4
#define    EXT_BOARD_IS_4CHIN_4CHOUT        5
#define    EXT_BOARD_IS_2CHIN_2CHOUT_BOX    6    //一个273，面积比较小
#define    EXT_BOARD_IS_2CHIN_2CHOUT_V1     7   //最老的，两个273
#define    EXT_BOARD_IS_2CHIN_2CHOUT_V2     8   //大板，第二版，一个273
#define    EXT_BOARD_IS_8CHIN_8CHOUT_V2     9   //带光耦的8路输入
#endif


//-------------------------------------------------------------------------------------------
//这里可以做一些系统配置
//另一些系统配置在sysdef.h文件里面

////放到Makefile中定义
//#define    BOARD_TYPE     EXT_BOARD_IS_2CHIN_2CHOUT_BOX
#define    RELEASE_OK   //发布？
//#define    DEBUG_ON

#if 0  //MAC地址一直到Makefile
#ifndef   RELEASE_OK
#define SYS_DEFAULT_MAC      "\x00\x06\x98\x30\x02\x0a"     //for test
#else
//0x01ec
#define SYS_DEFAULT_MAC      "\x00\x06\x98\x30\x01\xee"      //for release
#endif
#endif

//---------------------以下是485自动配置定义变量---------------------
//根据dev.nut文件生成的头文件定义的变量，来定义一下更多的宏
#include <cfg/platform_def.h>

#if 0  //一直到Makefile定义
#ifdef RELAY_PLATFORM_16CHOUT
#define  APP_485_ON
#else
#ifdef RELAY_PLATFORM_8CHIN_8CHOUT
#define  APP_485_ON
#else
#ifdef RELAY_PLATFORM_4CHIN_4CHOUT
#define  APP_485_ON
#else
#ifdef ETHERNET_TO_485_MIMIBOARD_V1
#define  APP_485_ON
#endif
#endif
#endif
#endif
#endif
//---------------------以上是485自动配置定义变量---------------------

#define USE_AUTO_CONFIG     //协议2   //是否开启 新的接口

//#define    CHANGE_IP_REBOOT_SYSTEM              //龚华青的版本,修改后即可重启，无需询问（一般不用,用命令重启）
#define    DEFINE_IO_TIMING_ON                  //是否有定时功能
//#define    DEFINE_POWER_RELAY_OLL_ON

#ifdef   DEBUG_ON
#define DEBUG_ON_INFO       1
#define DEBUG_ON_ERROR      1
#else
#define DEBUG_ON_INFO       0
#define DEBUG_ON_ERROR      0
#endif

#define ASSERT_MSG(assert,msg) do{ if(THISERROR)if(!(assert)) { printf msg;  printf(" at %s:%d\r\n",__FILE__,__LINE__); while(1); } }while(0)
//-------------------------------------------------------------------------------------------
//  以下不要随便修改
#if BOARD_TYPE != EXT_BOARD_IS_CAN485_MINIBOARD

#if BOARD_TYPE == EXT_BOARD_IS_6CHIN_7CHOUT || BOARD_TYPE == EXT_BOARD_IS_8CHIN_8CHOUT || BOARD_TYPE == EXT_BOARD_IS_4CHIN_4CHOUT || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_BOX || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V1 || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V2 || BOARD_TYPE ==EXT_BOARD_IS_8CHIN_8CHOUT_V2
#define  BOARD_INPUT8
#endif

#if  BOARD_TYPE == EXT_BOARD_IS_6CHIN_7CHOUT || BOARD_TYPE == EXT_BOARD_IS_8CHIN_8CHOUT  || BOARD_TYPE == EXT_BOARD_IS_16CHOUT || BOARD_TYPE == EXT_BOARD_IS_4CHIN_4CHOUT || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_BOX || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V1 || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V2 || BOARD_TYPE ==EXT_BOARD_IS_8CHIN_8CHOUT_V2
#define  BOARD_OUTPUT8
#endif

#if  BOARD_TYPE == EXT_BOARD_IS_16CHOUT
#define  BOARD_OUTPUT16
#endif

#if 0
#define  BOARD_OUTPUT24
#define  BOARD_OUTPUT32
#endif



#if BOARD_TYPE == EXT_BOARD_IS_6CHIN_7CHOUT
#define   OUTPUT_NO_CHANNEL_1
#endif



#if   BOARD_TYPE == EXT_BOARD_IS_4CHIN_4CHOUT ||  BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_BOX || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V2
#define   ONE_273_INPUT_MODE
#endif



#if   BOARD_TYPE == EXT_BOARD_IS_4CHIN_4CHOUT
#define INPUT_CHANNEL_NUM       4
#define CTL_INPUT1
#define CTL_INPUT2
#define CTL_INPUT3
#define CTL_INPUT4
#define OUTPUT_CHANNEL_NUM       4
#define RELAY_OUTPUT1
#define RELAY_OUTPUT2
#define RELAY_OUTPUT3
#define RELAY_OUTPUT4
#endif
#if   BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_BOX || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V1 || BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_V2
#define INPUT_CHANNEL_NUM        2
#define INPUT1
#define INPUT2
#define OUTPUT_CHANNEL_NUM       2
#define RELAY_OUTPUT1
#define RELAY_OUTPUT2
#endif
#if   BOARD_TYPE == EXT_BOARD_IS_6CHIN_7CHOUT
#define INPUT_CHANNEL_NUM        6
#define CTL_INPUT1
#define CTL_INPUT2
#define CTL_INPUT3
#define CTL_INPUT4
#define CTL_INPUT5
#define CTL_INPUT6
#define OUTPUT_CHANNEL_NUM       7
#define RELAY_OUTPUT1
#define RELAY_OUTPUT2
#define RELAY_OUTPUT3
#define RELAY_OUTPUT4
#define RELAY_OUTPUT5
#define RELAY_OUTPUT6
#define RELAY_OUTPUT7
#endif
#if   BOARD_TYPE == EXT_BOARD_IS_8CHIN_8CHOUT || BOARD_TYPE ==EXT_BOARD_IS_8CHIN_8CHOUT_V2
#define INPUT_CHANNEL_NUM        8
#define CTL_INPUT1
#define CTL_INPUT2
#define CTL_INPUT3
#define CTL_INPUT4
#define CTL_INPUT5
#define CTL_INPUT6
#define CTL_INPUT7
#define CTL_INPUT8
#define OUTPUT_CHANNEL_NUM       8
#define RELAY_OUTPUT1
#define RELAY_OUTPUT2
#define RELAY_OUTPUT3
#define RELAY_OUTPUT4
#define RELAY_OUTPUT5
#define RELAY_OUTPUT6
#define RELAY_OUTPUT7
#define RELAY_OUTPUT8
#endif

#if   BOARD_TYPE == EXT_BOARD_IS_16CHOUT
#define INPUT_CHANNEL_NUM        0
#define OUTPUT_CHANNEL_NUM       16
#define RELAY_OUTPUT1
#define RELAY_OUTPUT2
#define RELAY_OUTPUT3
#define RELAY_OUTPUT4
#define RELAY_OUTPUT5
#define RELAY_OUTPUT6
#define RELAY_OUTPUT7
#define RELAY_OUTPUT8
#define RELAY_OUTPUT9
#define RELAY_OUTPUT10
#define RELAY_OUTPUT11
#define RELAY_OUTPUT12
#define RELAY_OUTPUT13
#define RELAY_OUTPUT14
#define RELAY_OUTPUT15
#define RELAY_OUTPUT16
#endif


#if BOARD_TYPE ==EXT_BOARD_IS_8CHIN_8CHOUT_V2
#define INPUT_LEVEL_HITH_VALID
#endif


#if    BOARD_TYPE ==  EXT_BOARD_IS_2CHIN_2CHOUT_BOX
 //输入端1,2脚调换
#define SWAP_INPUT_PIN12
#define SWAP_CONFIG_PIN2_MODE
#endif


#else
//这里定义485开发板的定义
#endif
//-----------------------------------------------------------------------------------



#include "bin_command_def.h"
#include "io_time_ctl.h"
#include "cgi_thread.h"

extern uint16_t gwork_port;
extern uint16_t gweb_port;


uint32_t group_arry4_to_uint32(const uint8_t arry[4]);


void BspDebugLedInit(void);
void BspDebugLedSet(uint8_t ledmsk);
void IoPortInit(void);
void IoPortOut(uint32_t out);
uint8_t IoPortIn(void);
//------------------------
uint8_t IoGetConfig(void);
//
void SetRelayWithDelay(uint32_t out);
uint32_t GetIoOut(void);
uint32_t GetFilterInput(void);
//
int BspManualCtlModeInit(void);
int BspReadManualCtlModeIndex(unsigned char index,unsigned char * mode);
int BspWriteManualCtlModeIndex(unsigned char index,unsigned char mode);

void BspIoOutInit(void);
void BspIoInInit(void);
int BspAvrResetType(void);
#define AVR_JTAG_RESET         (1<<4)
#define AVR_WDT_RESET          (1<<3)
#define AVR_POWER_DOWN_RESET   (1<<2)
#define AVR_EXT_RESET          (1<<1)
#define AVR_POWER_UP_RESET     (1<<0)


//

#define INPUT_NUM                    32
#define OUTPUT_NUM                   32

#define  SYS_TIMING_COUNT_MAX        80

#define BSP_IP_ADDR_OFFSET           128


#define BSP_MODE_INDEX_OFFSET    (BSP_IP_ADDR_OFFSET+sizeof(CmdIpConfigData))
extern unsigned char switch_input_control_mode[32];




#define BSP_BOARD_INFO_OFFSET       (BSP_MODE_INDEX_OFFSET+sizeof(switch_input_control_mode))
#define HOST_ADDRESS_CONFIG         (BSP_BOARD_INFO_OFFSET+sizeof(CmdBoardInfo))
#define BSP_KEY_OFFSET              (HOST_ADDRESS_CONFIG+sizeof(host_address))

#ifdef APP_CGI_ON
#define BSP_IO_NAME_OFFSET          (BSP_KEY_OFFSET+sizeof(gpassword))
#else
#define BSP_IO_NAME_OFFSET          (BSP_KEY_OFFSET+0)
#endif

#define BSP_IO_TIMING_OFFSET        (BSP_IO_NAME_OFFSET+sizeof(CmdIoName)*(INPUT_NUM+OUTPUT_NUM))

#ifdef APP_TIMEIMG_ON
#define BSP_IO_SERIAL_ADDR_OFFSET   (BSP_IO_TIMING_OFFSET + sizeof(timing_node_eeprom)*SYS_TIMING_COUNT_MAX)
#else
#define BSP_IO_SERIAL_ADDR_OFFSET   (BSP_IO_TIMING_OFFSET + 0)
#endif


#define BSP_MAX_OFFSET              (BSP_IO_TIMING_OFFSET + 2)  //串口地址是2个字节




int BspReadWebPassword(char *);
int BspWriteWebPassword(char *);

extern void BspWriteEepromSerialAddress(uint16_t address);
extern uint16_t BspReadEepromSerialAddress(void);


int BspReadIpConfig(CmdIpConfigData * cid);
int BspWriteIpConfig(CmdIpConfigData * cid);
int BspReadHostAddress(host_address * info);
int BspWriteHostAddress(host_address * info);


int BspHostAddressChanged(void);

#ifdef ONE_273_INPUT_MODE
void bsp_set_output_value(uint32_t out);
uint32_t bsp_get_input_value(void);
void clock_out_clock_in(void);
unsigned char clock_out_clock_in2(unsigned char out);
void bsp_input_output_init(void);
#endif


#endif


