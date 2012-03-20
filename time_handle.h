#ifndef __TIME_HANDLE_H__
#define __TIME_HANDLE_H__

#include <time.h>
#include <sys/semaphore.h>



extern uint16_t    sys_time_ms;
extern struct _tm  sys_time;
extern uint8_t     sys_time_week;
extern SEM         sys_10ms_sem;
extern SEM         sys_second_sem;

typedef struct _now_time
{
	int         sys_time_ms;
	struct _tm  sys_time;
} now_time;

int get_week_day(uint16_t year,uint16_t month,uint16_t day);
int is_leap_year(int iyear);
int day_number(int iyear,int imon);
int  sys_time_init(void);
int time_add_millisecond(uint16_t ms);
now_time get_system_time(void);
struct _tm get_system_rtc_time(void);
int update_new_rtc_value(time_type * rio);
int raed_system_time_value(time_type * rio);


#endif

