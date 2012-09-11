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
#include <sys/mutex.h>
#include <stdio.h>

#include "bin_command_def.h"
#include "bsp.h"
#include "time_handle.h"

#include "bsp.h"

#define THISINFO           0
#define THISERROR          0


MUTEX       sys_time_lock;
uint16_t    sys_time_ms;
struct _tm  sys_time;
uint8_t     sys_time_week;

SEM         sys_10ms_sem;
SEM         sys_second_sem;




int is_leap_year(int iyear)
{
	uint16_t year = iyear;
	if(year % 4 == 0) {
		return 0;
	}
	if((year % 100 == 0) && (year % 400 == 0)) {
		return 0;
	}
	return -1;
}
int day_number(int iyear,int imon)
{
	uint16_t year,mon;
	year = iyear;
	mon = imon;
	if(!is_leap_year(year) && imon == 2) {
		return 29;
	} else {
		switch(imon)
		{
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		case 2:
			return 28;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		default:
			return 0;
		}
	}
}

int get_week_day(uint16_t year,uint16_t month,uint16_t day)
{
	uint8_t weekday,century,year_temp;

	century = year / 100;
	year_temp = year % 100;
	weekday = century/4 - 2*century + year_temp + year_temp/4 + 13 * (month + 1) / 5 + day;
	if(year<=1582 && month <= 10 && day <= 4)
	{
		weekday += 3;
		weekday %= 7;	
	}
	else
	{
		weekday -= 1;
		weekday %= 7;			
	}
	return weekday;
}



static void SetLocalTime(struct _tm * ltm)
{
    time_t now;
    now = mktime(ltm);
    stime(&now);
}


int  sys_time_init(void)
{
	sys_time_ms = 0;
	NutMutexInit(&sys_time_lock);

	NutSemInit(&sys_10ms_sem,0);
	NutSemInit(&sys_second_sem,0);

    /* Register and query hardware RTC, if available. */
    if(THISINFO)printf("Registering RTC hardware...");
    if (NutRegisterRtc(&RTC_CHIP)) {
        if(THISERROR)puts("NutRegisterRtc failed");
    } else {
		if(THISINFO)puts("NutRegisterRtc OK");
    }
	

#if 0
	{
		  sys_time.tm_sec = 0;
		  sys_time.tm_min = 39;
		  sys_time.tm_hour = 5;
		  sys_time.tm_mday = 16;
		  sys_time.tm_mon = 8;
		  sys_time.tm_year = 111;
		  SetLocalTime(&sys_time);
		  if(NutRtcSetTime(&sys_time)) {
			  if(NutRtcSetTime(&sys_time)) {
			  }
		  }
		  
	}
#endif


	if(NutRtcGetTime(&sys_time)) {
		if(NutRtcGetTime(&sys_time)) {
			if(NutRtcGetTime(&sys_time)) {
				if(THISERROR)printf("NutRtcSetTime failed!init system time failed!!!\r\n");
				goto error;
			}
		}
	}
	if(sys_time.tm_year < 111) {
error:
		  sys_time.tm_sec = 0;
		  sys_time.tm_min = 0;
		  sys_time.tm_hour = 0;
		  sys_time.tm_mday = 1;
		  sys_time.tm_mon = 0;
		  sys_time.tm_year = 111;
		  SetLocalTime(&sys_time);
		  if(NutRtcSetTime(&sys_time)) {
			  if(NutRtcSetTime(&sys_time)) {
				  return -1;
			  }
		  }
	}

	if(THISINFO) {
		printf("sec(%d),min(%d),hour(%d),mday(%d),mon(%d),year(%d),week(%d)\r\n",
			sys_time.tm_sec,sys_time.tm_min,sys_time.tm_hour,sys_time.tm_mday,sys_time.tm_mon,sys_time.tm_year+111,sys_time.tm_wday);
	}

	//sys_time.tm_sec -= 2;
	return 0;
}

int update_new_rtc_value(time_type * rio)
{
	int ret = 0;
	NutMutexLock(&sys_time_lock);
	sys_time.tm_sec = rio->sec;
	sys_time.tm_min = rio->min;
	sys_time.tm_hour = rio->hour;
	sys_time.tm_mday = rio->day;
	sys_time.tm_mon = rio->mon;
	sys_time.tm_year = rio->year;
	sys_time_ms = 0;
	SetLocalTime(&sys_time);
	if(!NutRtcSetTime(&sys_time)) {
		if(THISINFO)printf("Wirte new rtc value successful!\r\n");
	} else {
		if(!NutRtcSetTime(&sys_time)) {
			if(THISINFO)printf("Wirte new rtc value successful!\r\n");
		} else {
			if(!NutRtcSetTime(&sys_time)) {
			    if(THISINFO)printf("Wirte new rtc value successful!\r\n");
		    } else {
			    if(THISERROR)printf("Wirte new rtc value error!\r\n");
				ret = -1;
			}
		}
	}
	NutMutexUnlock(&sys_time_lock);
	return ret;
}

int raed_system_time_value(time_type * rio)
{
	NutMutexLock(&sys_time_lock);
	rio->sec = sys_time.tm_sec;
	rio->min = sys_time.tm_min;
	rio->hour = sys_time.tm_hour;
	rio->day = sys_time.tm_mday;
	rio->mon = sys_time.tm_mon;
	rio->year = sys_time.tm_year;
	NutMutexUnlock(&sys_time_lock);
	return 0;
}


//返回1为快了，-1为慢了，0为不快不慢
int time_add_millisecond(uint16_t ms)
{
	struct _tm  t,tmp;
	int         direction = 0;
	NutMutexLock(&sys_time_lock);
	sys_time_ms += ms;
	if(sys_time_ms >= 1000) {
		sys_time_ms -= 1000;
		direction = 1;
		//if(THISINFO)printf("second tick\r\n");
		if(++sys_time.tm_sec >= 60) {
			sys_time.tm_sec = 0;
			if(++sys_time.tm_min >= 60) {
				sys_time.tm_min = 0;
				if(++sys_time.tm_hour >= 24) {
					sys_time.tm_hour = 0;
					if(++sys_time.tm_mday > day_number(sys_time.tm_year+1900,sys_time.tm_mon+1)) {
						sys_time.tm_mday = 1;
						if(++sys_time.tm_mon >= 12) {
							sys_time.tm_mon = 0;
							++sys_time.tm_year;
						}
					}
				}
			}
		}
	}
	tmp = sys_time;
	NutMutexUnlock(&sys_time_lock);
	if(direction) {
		NutSemPost(&sys_second_sem);
	    if(NutRtcGetTime(&t)) {
			//读系统RTC
			time_t tt = time(NULL);
			memcpy(&t,localtime(&tt),sizeof(struct _tm));
			direction = 1;
			//if(THISINFO)printf("GetRTC Failed!,GetLoaclTime OK\r\n");
		} else {
			//读取系统时间成功，更新星期
			sys_time_week = t.tm_wday;
		}
	}
	//
	if(direction) {
		direction = 0;
	    //校准时间
		//if(THISINFO)printf("do cal time\r\n");
	    if(tmp.tm_year > t.tm_year) { //快了
		    direction = 1;
	    } else if(tmp.tm_year < t.tm_year) {
	    	direction = -1;
	    } else {
		    //相等,判断月份
		    if(tmp.tm_mon > t.tm_mon) {
				direction = 1;
		    } else if(tmp.tm_mon < t.tm_mon) {
				direction = -1;
		    } else {
				if(tmp.tm_mday > t.tm_mday) {
					direction = 1;
				} else if(tmp.tm_mday < t.tm_mday) {
					direction = -1;
				} else {
					if(tmp.tm_hour > t.tm_hour) {
						direction = 1;
					} else if(tmp.tm_hour < t.tm_hour) {
						direction = -1;
					} else {
						if(tmp.tm_min > t.tm_min) {
							direction = 1;
						} else if(tmp.tm_min < t.tm_min) {
							direction = -1;
						} else {
							if(tmp.tm_sec > t.tm_sec) {
								direction = 1;
							} else if(tmp.tm_sec < t.tm_sec) {
								direction = -1;
							} else {
								//剩下毫秒没办法比较
								//if(THISINFO)printf("no fast slow.\r\n");
							}
						}
					}
				}
		   }
	    }
	}
	NutSemPost(&sys_10ms_sem);
	return direction;
}

now_time get_system_time(void)
{
	now_time  t;
	NutMutexLock(&sys_time_lock);
	t.sys_time = sys_time;
	t.sys_time_ms = sys_time_ms;
	NutMutexUnlock(&sys_time_lock);
	return t;
}
struct _tm get_system_rtc_time(void)
{
	struct _tm  t;
	if(NutRtcGetTime(&t)) {
		time_t tt = time(NULL);
		//if(THISINFO)printf("In get_system_rtc_time Get Lockatime\r\n");
		memcpy(&t,localtime(&tt),sizeof(struct _tm));
	} else {
		//if(THISINFO)printf("Nut Rtc Get Time OK\r\n");
	}
	return t;
}
