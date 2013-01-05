
#------------------------------------------------------------------------------
#过期的板类型
#
EXT_BOARD_IS_6CHIN_7CHOUT   =     1
EXT_BOARD_IS_8CHIN_8CHOUT   =     2
EXT_BOARD_IS_CAN485_MINIBOARD  =  3

#最老的，两个273   
EXT_BOARD_IS_2CHIN_2CHOUT_V1  =   7 

# //大板，第二版，一个273  
EXT_BOARD_IS_2CHIN_2CHOUT_V2  =   8  

#------------------------------------------------------------------------------
#继续维护的板类型
#
#一个273，面积比较小
EXT_BOARD_IS_2CHIN_2CHOUT_BOX  =  6
#16路输出
EXT_BOARD_IS_16CHOUT           =  4
#4进4出
EXT_BOARD_IS_4CHIN_4CHOUT    =    5
#带光耦的8路输入，8路输出
EXT_BOARD_IS_8CHIN_8CHOUT_V2  =   9
#30安16进16出带串口和485的继电器控制板
RELAY_PLATFORM_16CHIN_16CHOUT_30A = 11
#以太网服务器远程复位开关控制卡
RELAY_PLATFORM_16CHOUT_HOST_RESET = 10
#
#定义当前需要编译的板类型
BSP_BOARD_TYPE = $(RELAY_PLATFORM_16CHIN_16CHOUT_30A)

#
#发布MAC地址
MAC0 = 00
MAC1 = 06
MAC2 = 90
MAC3 = 13
MAC4 = 01
MAC5 = 03
#以上MAC地址将自动生成一下字符串
ETHERNET_MAC = \"\x$(MAC0)\x$(MAC1)\x$(MAC2)\x$(MAC3)\x$(MAC4)\x$(MAC5)\"
HWDEF += -DSYS_DEFAULT_MAC=$(ETHERNET_MAC)
HWDEF += -DBOARD_TYPE_MODEL=$(BSP_BOARD_TYPE)

#定义一些模块开关，删除ON，就取消某功能
APP_TIMEIMG_ON = ON
APP_CGI_ON = ON
APP_485_ON = ON

#扩展后16路到串口继电器，如果关闭，则485作为串口输入，打开则为扩展输出
APP_485_EXTOUT = ON

APP_MODBUS_TCP_ON  = ON
APP_MULTI_MANGER_FRAME = ON
#删除ON，PHP功能就取消
APP_HTTP_PROTOTOL_CLIENT = 
#------------------------------------------------------------------------------








#以下脚本不能随意更改
#------------------------------------------------------------------------------

MAC_STRING = $(MAC0)_$(MAC1)_$(MAC2)_$(MAC3)_$(MAC4)_$(MAC5)


ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_2CHIN_2CHOUT_BOX))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_2CHIN_2CHOUT_BOX)
WEBDIR  = web_2ch
HEX_FILE = 2ch_$(MAC_STRING)
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_4CHIN_4CHOUT))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_4CHIN_4CHOUT)
SYS_HAVE_485 = ON
WEBDIR  = web_4ch
HEX_FILE = 4ch_$(MAC_STRING)
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_8CHIN_8CHOUT_V2))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_8CHIN_8CHOUT_V2)
SYS_HAVE_485 = ON
WEBDIR  = web_8ch
HEX_FILE = 8ch_$(MAC_STRING)
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_16CHOUT))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_16CHOUT)
SYS_HAVE_485 = ON
WEBDIR  = web_16ch
HEX_FILE = 16ch_$(MAC_STRING)
endif

ifeq ($(BSP_BOARD_TYPE),$(RELAY_PLATFORM_16CHIN_16CHOUT_30A))
HWDEF += -DBOARD_TYPE=$(RELAY_PLATFORM_16CHIN_16CHOUT_30A)
SYS_HAVE_485 = ON
WEBDIR  = web_16ch
HEX_FILE = 16in_16out_$(MAC_STRING)
endif


ifeq ($(BSP_BOARD_TYPE),$(RELAY_PLATFORM_16CHOUT_HOST_RESET))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_16CHOUT)
WEBDIR  = web_16ch
HEX_FILE = host_reset_16out_$(MAC_STRING)
endif


#------------------------------------------------------------------------------






PROJ = $(HEX_FILE)
WEBFILE = urom.c

SRCS = plc_io_inter.c StringPrecess.c plc_prase.c main.c io_out.c bin_cmd_server.c bsp.c  time_handle.c udp_client_command.c app_check_pro.c regtable.c


#485两个条件必须满足才编译
ifeq ($(SYS_HAVE_485),ON)
ifeq ($(APP_485_ON),ON)
ifeq ($(APP_485_EXTOUT),ON)
SRCS := $(SRCS) extern_serial16ch_out.c
HWDEF += -DAPP_485_ON
HWDEF += -DAPP_485_EXTOUT
else
SRCS := $(SRCS) can_485_uart.c
HWDEF += -DAPP_485_ON
endif
endif
endif

ifeq ($(APP_TIMEIMG_ON),ON)
SRCS := $(SRCS) io_time_ctl.c
HWDEF += -DAPP_TIMEIMG_ON
endif

ifeq ($(APP_CGI_ON),ON)
SRCS := $(SRCS) cgi_thread.c urom.c
HWDEF += -DAPP_CGI_ON
endif


ifeq ($(APP_MODBUS_TCP_ON),ON)
SRCS := $(SRCS) modbus_interface.c
HWDEF += -DAPP_MODBUS_TCP_ON
endif


ifeq ($(APP_MULTI_MANGER_FRAME),ON)
SRCS := $(SRCS) rc4.c multimgr_device.c
HWDEF += -DAPP_MULTI_MANGER_FRAME
endif

ifeq ($(APP_HTTP_PROTOTOL_CLIENT),ON)
SRCS := $(SRCS) StringPrecess.c http_request.c
HWDEF += -DAPP_HTTP_PROTOTOL_CLIENT
endif


include ../Makedefs


OBJS =  $(SRCS:.c=.o)
LIBS =  $(LIBDIR)/nutinit.o -lnutpro -lnutos -lnutarch -lnutdev -lnutgorp -lnutnet -lnutfs -lnutcrt $(ADDLIBS)
TARG =  $(PROJ).hex


DTARG := $(DTARG)

all: $(OBJS) $(TARG) $(ITARG) $(DTARG)
$(WEBFILE): $(WEBDIR)/index.html $(WEBDIR)/io_out_control.html  $(WEBDIR)/main.html 
	$(CRUROM) -r -o$(WEBFILE) $(WEBDIR)

include ../Makerules



clean:
	-rm -f $(OBJS)
	-rm -f $(TARG) $(ITARG) $(DTARG)
	-rm -f $(PROJ).eep
	-rm -f $(PROJ).obj
	-rm -f $(PROJ).map
	-rm -f $(SRCS:.c=.lst)
	-rm -f $(SRCS:.c=.bak)
	-rm -f $(SRCS:.c=.i)
	-rm -f urom.c urom.o urom.lst
