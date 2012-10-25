#ifndef __BIN_COMMAND_H__
#define __BIN_COMMAND_H__

typedef   uint8_t   u8_t;
typedef   uint16_t  u16_t;
typedef   uint32_t  u32_t;

//成功与否，实际上只占一位
#define   CMD_ACK_OK                  0x01
#define   CMD_ACK_KO                  0x00

#define  CMD_REQ_START             0
#define  CMD_REQ_NEXT              1
#define  CMD_REQ_DONE              2

#define  CMD_CURRENT_START        0
#define  CMD_CURRENT_DOING        1
#define  CMD_CURRENT_DONE         2

extern uint8_t    command_state;

#define   CMD_GET_IO_OUT_VALUE           1
#define   CMD_GET_IO_IN_VALUE            2
#define   CMD_SET_IO_OUT_VALUE           3
#define   CMD_GET_IO_NAME                4
#define   CMD_SET_IO_NAME                5
#define   CMD_GET_TIMING_INFO            6
#define   CMD_SET_TIMING_INFO            7
#define   CMD_GET_RTC_VALUE              8
#define   CMD_SET_RTC_VALUE              9
#define   CMD_SET_TIMING_ON_MSK          10
#define   CMD_GET_TIMING_ON_MSK          11
#define   CMD_SET_INPUT_CTL_ON_MSK       12
#define   CMD_GET_INPUT_CTL_ON_MSK       13
#define   CMD_SET_HOST_ADDRESS           14
#define   CMD_GET_HOST_ADDRESS           15
#define   CMD_SET_SYSTEM_RESET           16
#define   CMD_REV_IO_SOME_BIT            36
#define   CMD_SET_IO_ONE_BIT             37 //
#define   CMD_CLR_IO_ONE_BIT             38 //
#define   CMD_REV_IO_ONE_BIT             39 //
#define   CMD_SET_IP_CONFIG              33
#define   CMD_GET_IP_CONFIG              34
#define   CMD_GET_INPUT_CTL_MODE_INDEX   40
#define   CMD_SET_INPUT_CTL_MODE_INDEX   41

#if 0
typedef struct _CmdHead
{
  uint8_t  cmd;
  uint16_t cmd_index;
  uint8_t  cmd_option;
  uint16_t cmd_len;
  uint8_t  data_checksum;
} CmdHead;
#endif
typedef struct _CmdHead
{
  uint8_t  cmd;
  uint16_t cmd_index;
  uint8_t  cmd_option;
  uint8_t  pad;
  uint8_t  cmd_len;
  uint8_t  data_checksum;
} CmdHead;

#define GET_CMD_DATA(pCmd)         ((void *)(((char *)pCmd)+sizeof(CmdHead)))
#define GET_CMD_OK(pcmd)           (((pcmd)->cmd_option)&CMD_ACK_OK)
#define SET_CMD_OK(pcmd,ok)        do{if(!ok){(pcmd)->cmd_option &= ~CMD_ACK_OK;}else{(pcmd)->cmd_option |= CMD_ACK_OK;}}while(0)
#define GET_CMD_STATE(pcmd)        ((((pcmd)->cmd_option)&0xF0)>>4)
#define SET_CMD_STATE(pcmd,mode)   do{(pcmd)->cmd_option &= ~0xF0;(pcmd)->cmd_option |= ((mode)&0xF)<<4; }while(0)

typedef struct _CmdIoOneBit
{
	uint8_t  io_bitnum[2];
} CmdIoOneBit;


typedef struct _CmdIoIndex
{
  uint8_t    io_msk[4];
} CmdIobitmap;

typedef struct _CmdMode
{
  uint8_t    mode;
} CmdMode;

typedef struct _CmdInputMode
{
  uint8_t    mode[8];
} CmdInputMode;

typedef struct _CmdInputModeIndex
{
	uint8_t    index;
    uint8_t    mode;
} CmdInputModeIndex;

#define INPUT_TRIGGER_FLIP_MODE          0x00  //触发反转模式
#define INPUT_SINGLE_TRIGGER_MODE        0x01  //单触发模式
#define INPUT_TRIGGER_TO_OPEN_MODE       0x02  //触发开通模式
#define INPUT_TRIGGER_TO_OFF_MODE        0x03  //触发关闭模式
#define INPUT_EDGE_TRIG_MODE             0x04  //边沿触发模式
#define INPUT_LEVEL_CTL_ON_MODE          0x05  //输入电平控制开模式
#define INPUT_LEVEL_CTL_OFF_MODE         0x06  //输入电平控制关模式
#define INPUT_TRIGGER_OFF_MODE           0x07  //控制关闭

typedef struct _CmdIoValidMsk
{
	uint8_t   valid_msk[4];
	uint8_t   invalid_msk[4];
} CmdIoValidMsk;

typedef struct _CmdIpConfigData
{
  uint8_t   ipaddr[4];
  uint8_t   netmask[4];
  uint8_t   gateway[4];
  uint8_t   dns[4];
  uint16_t  port;
  uint16_t  webport;
} CmdIpConfigData;


//加密功能
typedef struct _CmdMcuId
{
  uint8_t  IdLength;
  uint8_t  IdValue[1];
} CmdMcuId;

typedef struct _CmdIoValue
{
  uint8_t    io_count;
  uint8_t    io_value[4];
} CmdIoValue;

typedef struct _CmdIoBit
{
  uint8_t    io_index;
  uint8_t    io_enable;
} CmdIoBit;

typedef struct _CmdBoardInfo
{
	uint8_t  version;
	uint8_t  option; //是否带锁
	uint8_t  board_name[20];
	uint8_t  password[20];
} CmdBoardInfo;

typedef struct _CmdIoName
{
	uint8_t   io_addr[2];
	uint8_t   io_name[20];
} CmdIoName;

//定时的读写时序描述
//定义定时时间参数
typedef struct _time_type
{
	uint8_t    year;
	uint8_t    mon;
	uint8_t    day;
	uint8_t    hour;
	uint8_t    min;
	uint8_t    sec;
} time_type;
typedef struct _timing_node
{
	uint8_t    addr[2];  //定时端口地址编码（板上输出口地址范围为[0][0] ~ [0][16]），第5路输出就是addr[0]=4,addr[1]=0
	uint16_t   option;   //选项，第0位:1表示定时有效，0表示无效,第1位:1表示小循环开启，0关闭。第[8:11]位表示循环类型，类型值如下
	//
	time_type  start_time;  //循环开启时间（精确到秒，以板上实时时间为准）
	time_type  end_time;    //循环关闭时间（精确到秒，以板上实时时间为准）
	//
	uint32_t   duty_cycle;  //小循环中打开继电器的时间周期，即使开通时间长度,毫秒为单位，最小10毫秒，最长数十天（49天）
	uint32_t   period;     //小循环一个周期的时间长度,毫秒为单位，最小10毫秒，最长数十天（49天）
	//记录状态
	uint8_t    state;  //内部使用,写0
} timing_node;
//定时节点的个数，
typedef struct _timint_info
{
	uint16_t    time_max_count;
} timint_info;
//定义读写的时候定时数据正文
typedef struct _CmdTimingNode
{
	uint16_t    index;
	timing_node node;
} CmdTimingNode;
//定时只在时间到的时候触发输出控制，只有在开或关的瞬间发生触发
//多个定时节点对同一路输出控制，以最后一路（发送写命令为顺序）定时节点的触发信号会覆盖前面节点触发的状态。
//定时时间设定必须先写timint_info数据
//然后连续调用写CmdTimingNode命令，直到完成为止.
//CmdTimingNode钟字段index的范围是0-(time_max_count-1)
//写的过程分三个阶段，
//第一：SendCommand(CMD_SET_TIMING_INFO,CMD_REQ_START,data),data是数据正文，类型为timint_info
//第二阶段：SendCommand(CMD_SET_TIMING_INFO,CMD_REQ_NEXT,data),datas是正文，类型为CmdTimingNode,连续写完time_max_count个命令
//第三阶段:SendCommand(CMD_SET_TIMING_INFO,CMD_REQ_DONE,data);data是正文，长度为零，通知下位机完成写事务
//参数CMD_REQ_START是报文头的cmd_option的第[4:7]位，定义如下
//#define  CMD_REQ_START             0
//#define  CMD_REQ_NEXT              1
//#define  CMD_REQ_DONE              2
//应答包则对应的，也在cmd_option中设置了[4:7]位，表明上一命令的状态
//用C语言描述表示如下：
//#define GET_CMD_STATE(pcmd)        ((((pcmd)->cmd_option)&0xF0)>>4)
//#define SET_CMD_STATE(pcmd,mode)   do{(pcmd)->cmd_option &= ~0xF0;(pcmd)->cmd_option |= ((mode)&0xF)<<4; }while(0)
//-----------------
//读定时则和写定时时序相当，也是三个阶段，三个状态，只是数据命令不同而正文数据长度为零而已。
//各个定义如下:
//1,SendCommand(CMD_GET_TIMING_INFO,CMD_REQ_START,data);
//2,SendCommand(CMD_GET_TIMING_INFO,CMD_REQ_NEXT,data);
//3,SendCommand(CMD_GET_TIMING_INFO,CMD_REQ_DONE,data);
//注意，定时读的应答正文数据的格式是timing_node,而不是CmdTimingNode
//以下是各个参数的定义
//timing_node的option字段
#define  VALID       1
#define  INVALID     0
//state
#define  TIME_NO_COME    0
#define  TIME_DOING      1
#define  TIME_SUBDO_ON   2
#define  TIME_SUBDO_OFF  3
#define  TIME_OVER       4
//

//option
#define  CYCLE_YEAR      0
#define  CYCLE_MONTH     1
#define  CYCLE_DAY       2
#define  CYCLE_HOUR      3
#define  CYCLE_MINITH    4
#define  CYCLE_SECOND    5
#define  CYCLE_WEEK      6
#define  CYCLE_USER      7
#define  CYCLE_ONCE      8
//option
#define  TIME_VALID      0x01
#define  TIME_SUBCYCLE   0x02
#define  TIME_HOLD_IO    0x04
#define  TIME_DONE       0x08


//-------------------------------------------------------------
#define  SET_IO_TIME_VALID(ptime,valid)  \
	do{  \
	  if(valid) { (ptime)->option |=  TIME_VALID; } else { (ptime)->option &= ~TIME_VALID; }   \
	} while(0)
#define  GET_IO_TIME_VALID(ptime)   \
	     (((ptime)->option)&TIME_VALID)
//-------------------------------------------------------------
//-------------------------------------------------------------
#define  SET_IO_TIME_SUBCYCLEON(ptime,valid)  \
	do{  \
	  if(valid) { (ptime)->option |=  TIME_SUBCYCLE; } else { (ptime)->option &= ~TIME_SUBCYCLE; }   \
    } while(0)
#define  GET_IO_TIME_SUBCYCLEON(ptime)   \
	     ((((ptime)->option)&TIME_SUBCYCLE)?1:0)
//-------------------------------------------------------------
#define  SET_IO_TIME_HOLD_IO(ptime,valid)  \
	do{  \
	  if(valid) { (ptime)->option |=  TIME_HOLD_IO; } else { (ptime)->option &= ~TIME_HOLD_IO; }   \
    } while(0)
#define  GET_IO_TIME_HOLD_IO(ptime)   \
	     (((ptime)->option)&TIME_MS_TICK)
//-------------------------------------------------------------
//-------------------------------------------------------------
#define  SET_IO_TIME_CYCLE_TYPE(ptime,type)  \
	do{ (ptime)->option &= ~(0xF<<8);(ptime)->option |= (((type)&0xF)<<8); } while(0)
#define  GET_IO_TIME_CYCLE_TYPE(ptime)   \
	     ((((ptime)->option)>>8)&0xF)
//-------------------------------------------------------------
#define  SET_IO_TIME_IO_ON_OFF(ptime,valid)  \
	do{  \
	  if(valid) { (ptime)->option |=  TIME_ON_OFF; } else { (ptime)->option &= ~TIME_ON_OFF; }   \
    } while(0)
#define  GET_IO_TIME_IO_ON_OFF(ptime)   \
	     (((ptime)->option)&TIME_ON_OFF)
//-------------------------------------------------------------
#define  SET_IO_TIME_DONE(ptime,valid)  \
	do{  \
	  if(valid) { (ptime)->option |=  TIME_DONE; } else { (ptime)->option &= ~TIME_DONE; }   \
    } while(0)
#define  GET_IO_TIME_DONE(ptime)   \
	     (((ptime)->option)&TIME_DONE)


typedef struct _host_address
{
	u8_t     type;  //type = 0:192.168.1.111....,type = 1 : www.hostaddr.com
	char     host_address[64];
	u16_t    port;
} host_address;

#define  HOST_ADDR_IS_IP_MODE       0
#define  HOST_ADDR_IS_DOMAIN_MODE   1
//
#define  HOST_IS_UDP                0
#define  HOST_IS_TCP_SERVER         1
#define  HOST_IS_TCP_CLIENT         2

//-------------------------------------------------------------
#endif