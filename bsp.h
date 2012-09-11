#ifndef __NUT_BSP_H__
#define __NUT_BSP_H__


#define  EXT_BOARD_IS_2CHIN_2CHOUT_BOX       6
#define  EXT_BOARD_IS_16CHOUT                4
#define  EXT_BOARD_IS_4CHIN_4CHOUT           5
#define  EXT_BOARD_IS_8CHIN_8CHOUT_V2        9  
#define  RELAY_PLATFORM_16CHIN_16CHOUT_30A   10
#define  RELAY_PLATFORM_16CHOUT_HOST_RESET   11


#if BOARD_TYPE == EXT_BOARD_IS_2CHIN_2CHOUT_BOX
#define INPUT_CHANNEL_NUM        2
#define OUTPUT_CHANNEL_NUM       2
//一些特定变更
#define ONE_273_INPUT_MODE
#define SWAP_INPUT_PIN12
#define SWAP_CONFIG_PIN2_MODE
#endif

#if BOARD_TYPE == EXT_BOARD_IS_16CHOUT
#define INPUT_CHANNEL_NUM        0
#define OUTPUT_CHANNEL_NUM       16
#endif

#if BOARD_TYPE == EXT_BOARD_IS_4CHIN_4CHOUT
#define INPUT_CHANNEL_NUM        4
#define OUTPUT_CHANNEL_NUM       4
#define   ONE_273_INPUT_MODE
#endif

#if BOARD_TYPE == EXT_BOARD_IS_8CHIN_8CHOUT_V2
#define INPUT_CHANNEL_NUM        8
#define OUTPUT_CHANNEL_NUM       8
#define INPUT_LEVEL_HITH_VALID
#endif

#if BOARD_TYPE == RELAY_PLATFORM_16CHIN_16CHOUT_30A
#define INPUT_CHANNEL_NUM        16
#define OUTPUT_CHANNEL_NUM       16
#endif

#if BOARD_TYPE == RELAY_PLATFORM_16CHOUT_HOST_RESET
#define INPUT_CHANNEL_NUM        0
#define OUTPUT_CHANNEL_NUM       16
#endif


/*_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_*/
#include "bin_command_def.h"
#include "io_time_ctl.h"
#include "cgi_thread.h"
#include "multimgr_device_dev.h"

extern uint16_t gwork_port;
extern uint16_t gweb_port;
extern unsigned char gconfig;


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

#define  SYS_TIMING_COUNT_MAX        60

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


//串口地址总是有的
#define BSP_MULTI_MANGER_DATA_OFFSET    (BSP_IO_SERIAL_ADDR_OFFSET + 2)  //串口地址是2个字节

#define BSP_FACTORY_OUT_OFFSET          (BSP_MULTI_MANGER_DATA_OFFSET + sizeof(device_info_st))


#ifdef APP_HTTP_PROTOTOL_CLIENT
#define BSP_HTTP_CLIENT_INFO_OFFSET     (BSP_FACTORY_OUT_OFFSET + 1)
#define BSP_MAX_OFFSET                  (BSP_HTTP_CLIENT_INFO_OFFSET + sizeof(ethernet_relay_info))  //在前面的基础上，增加前面的大小
#else
#define BSP_MAX_OFFSET                  (BSP_FACTORY_OUT_OFFSET + 1)
#endif




char BspReadFactoryOut(void);
void BspWriteFactoryOut(char ch);

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

#ifdef APP_MULTI_MANGER_FRAME
extern void BspLoadmultimgr_info(device_info_st * info);
extern void BspSavemultimgr_info(device_info_st * info);
#endif


#ifdef APP_HTTP_PROTOTOL_CLIENT
extern int load_relay_info(ethernet_relay_info * info);
extern int save_relay_info(ethernet_relay_info * info);
#endif


#endif


