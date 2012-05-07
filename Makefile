
#------------------------------------------------------------------------------
#����������
#
EXT_BOARD_IS_6CHIN_7CHOUT   =     1
EXT_BOARD_IS_8CHIN_8CHOUT   =     2
EXT_BOARD_IS_CAN485_MINIBOARD  =  3
EXT_BOARD_IS_16CHOUT           =  4
EXT_BOARD_IS_4CHIN_4CHOUT    =    5

#һ��273������Ƚ�С
EXT_BOARD_IS_2CHIN_2CHOUT_BOX  =  6 

#���ϵģ�����273   
EXT_BOARD_IS_2CHIN_2CHOUT_V1  =   7 

# //��壬�ڶ��棬һ��273  
EXT_BOARD_IS_2CHIN_2CHOUT_V2  =   8  

# //�������8·����
EXT_BOARD_IS_8CHIN_8CHOUT_V2  =   9  
#
#
#���������
BSP_BOARD_TYPE = $(EXT_BOARD_IS_16CHOUT)

#
#����MAC��ַ
#��������x01\xee,˫����Ҫ��\����
ETHERNET_MAC = \"\x00\x06\x98\x30\x01\xFE\"
HWDEF += -DSYS_DEFAULT_MAC=$(ETHERNET_MAC)

#����һЩģ�鿪��
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


#485����������������ű���
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
SRCS := $(SRCS) rc4.c
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
