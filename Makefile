
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
#
#定义当前需要编译的板类型
BSP_BOARD_TYPE = $(EXT_BOARD_IS_2CHIN_2CHOUT_BOX)

#
#发布MAC地址
#最后编码是x01\xee,双引号要用\引导
ETHERNET_MAC = \"\x00\x06\x98\x48x20\x59\"
HWDEF += -DSYS_DEFAULT_MAC=$(ETHERNET_MAC)
HWDEF += -DBOARD_TYPE_MODEL=$(BSP_BOARD_TYPE)

#定义一些模块开关
APP_TIMEIMG_ON = ON
APP_CGI_ON = ON
APP_485_ON = ON
APP_MODBUS_TCP_ON  = ON
APP_MULTI_MANGER_FRAME = ON
#------------------------------------------------------------------------------


#------------------------------------------------------------------------------





ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_2CHIN_2CHOUT_BOX))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_2CHIN_2CHOUT_BOX)
#SYS_HAVE_485 = ON
WEBDIR  = web_2ch
HEX_FILE = release_hex_2ch
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_4CHIN_4CHOUT))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_4CHIN_4CHOUT)
SYS_HAVE_485 = ON
WEBDIR  = web_4ch
HEX_FILE = release_hex_4ch
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_8CHIN_8CHOUT_V2))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_8CHIN_8CHOUT_V2)
SYS_HAVE_485 = ON
WEBDIR  = web_8ch
HEX_FILE = release_hex_8ch
endif

ifeq ($(BSP_BOARD_TYPE),$(EXT_BOARD_IS_16CHOUT))
HWDEF += -DBOARD_TYPE=$(EXT_BOARD_IS_16CHOUT)
SYS_HAVE_485 = ON
WEBDIR  = web_16ch
HEX_FILE = release_hex_16ch
endif

#------------------------------------------------------------------------------






PROJ = $(HEX_FILE)
WEBFILE = urom.c

SRCS = main.c io_out.c bin_cmd_server.c bsp.c  time_handle.c udp_client_command.c app_check_pro.c


#485两个条件必须满足才编译
ifeq ($(SYS_HAVE_485),ON)
ifeq ($(APP_485_ON),ON)
SRCS := $(SRCS) can_485_uart.c
HWDEF += -DAPP_485_ON
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


include ../Makedefs


OBJS =  $(SRCS:.c=.o)
LIBS =  $(LIBDIR)/nutinit.o -lnutpro -lnutos -lnutarch -lnutdev -lnutgorp -lnutnet -lnutfs -lnutcrt $(ADDLIBS)
TARG =  $(PROJ).hex


DTARG := $(DTARG)

all: $(OBJS) $(TARG) $(ITARG) $(DTARG)
$(WEBFILE): $(WEBDIR)/index.html $(WEBDIR)/io_out_control.html
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
