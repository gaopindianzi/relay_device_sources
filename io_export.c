#include <compiler.h>
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
#include <netdb.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/watchdog.h>
#include <sys/timer.h>
#include <dev/ds1307rtc.h>
#include <dev/rtc.h>
#include <time.h>
#include <dev/gpio.h>


#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bin_command_def.h"
#include "bsp.h"
#include "sysdef.h"
#include "cgi_thread.h"
#include "server.h"
#include "time_handle.h"
#include <dev/usartavr485.h>
#include <dev/relaycontrol.h>
#include <cfg/platform_def.h>
#include "io_out.h"
#include "sys_var.h"
#include "bsp.h"

#ifdef   APP_EXTEND_IO

#define THISINFO        0
#define THISERROR       0


THREAD(extend_io_thread, arg)
{
	unsigned char tx_buffer[9];
	modbus_head  * pcmd = (modbus_head *)tx_buffer;
	st_relay_msk *  pst = (st_relay_msk *)&(tx_buffer[sizeof(modbus_head)]);
	//构造发送数据结构
	pcmd->pad0 = 0x00;
	pcmd->pad1 = 0x5A;
	pcmd->group_addr = 0;
	pcmd->dev_addr = 0;
	pcmd->command = MODBUS_SET_RELAY;
	pcmd->data_len = 2;
	NutThreadSetPriority(IO_EXTEND_THREAD_PRI);
	if(THISINFO)printf("extend_io_thread is running...\r\n");
	while(1)
	{
		if(sys_varient.iofile) {
			if(sys_varient.pioout) {
				//发送485数据
				if(sys_varient.stream_max485) {
					//构建发送程序
					//有一个BUG，也许是串口的....
					pst->ioary[0] = sys_io.pioout[3]; //从16路开始
					pst->ioary[1] = sys_io.pioout[2]; //从16路开始
					tx_buffer[8] = check_sum(tx_buffer,sizeof(tx_buffer)-1);
					fwrite(tx_buffer,sizeof(char),sizeof(tx_buffer),sys_varient.stream_max485);
				} else {
					if(THISERROR)printf("sys_io.stream_max485 not opened!\r\n");
				}
			} else {
				if(THISERROR)printf("sys_io.pioout == NULL error!\r\n");
			}
		} else {
			if(THISERROR)printf("sys_io.iofile not opened!\r\n");
		}
        NutEventWait(&(sys_io.io_out_event), 1000);
	}
}

void StartExtendIoThread(void)
{
	uint32_t baud = 9600;
    _ioctl(_fileno(sys_varient.stream_max485), UART_SETSPEED, &baud);
	baud = 5; //10ms
	_ioctl(_fileno(sys_varient.stream_max485), UART_SETREADTIMEOUT, &baud);
	if(THISINFO)printf("Start extend_io_thread...\r\n");
    NutThreadCreate("extend_io_thread",  extend_io_thread, 0, 1024);
}

#endif //APP_EXTEND_IO