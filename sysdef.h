#ifndef __SYSDEF_H__
#define __SYSDEF_H__

#define USE_DHCP
#define USE_DISCOVERY


#ifdef USE_PHAT

#if defined(ETHERNUT3)

/* Ethernut 3 file system. */
#define MY_FSDEV       devPhat0
#define MY_FSDEV_NAME  "PHAT0" 

/* Ethernut 3 block device interface. */
#define MY_BLKDEV      devNplMmc0
#define MY_BLKDEV_NAME "MMC0"

#elif defined(AT91SAM7X_EK)

/* SAM7X-EK file system. */
#define MY_FSDEV       devPhat0
#define MY_FSDEV_NAME  "PHAT0" 

/* SAM7X-EK block device interface. */
#define MY_BLKDEV      devAt91SpiMmc0
#define MY_BLKDEV_NAME "MMC0"

#elif defined(AT91SAM9260_EK)

/* SAM9260-EK file system. */
#define MY_FSDEV       devPhat0
#define MY_FSDEV_NAME  "PHAT0" 

/* SAM9260-EK block device interface. */
#define MY_BLKDEV      devAt91Mci0
#define MY_BLKDEV_NAME "MCI0"

#endif
#endif /* USE_PHAT */

#ifndef MY_FSDEV
#define MY_FSDEV        devUrom
#endif

#ifdef MY_FSDEV_NAME
#define MY_HTTPROOT     MY_FSDEV_NAME ":/" 
#endif

#include "bsp.h"



#define MAIN_RTC_TIME_PRI           100
#define TCP_BIN_SERVER_PRI          101
#define AUTO_CONFIG_THREAD_PRI      102
#define UDP_CLIENT_SERVER_PRI       103
#define IO_AND_TIMING_SCAN_RPI      200  //不能太高，任务重



//端口
#define UDP_AUTO_CONFIG_PORT        6799

#endif