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
#include <errno.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>

#include <sys/mutex.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif


#include "bin_command_def.h"
#include "bsp.h"
#include "time_handle.h"
#include "io_time_ctl.h"
#include "time_handle.h"
#include "io_out.h"
#include "bsp.h"
#include "debug.h"


#define THISINFO      0
#define THISERROR     0
#define THISASSERT    0

#define ASSERT_MSG(on,str)    ASSERT(on)


void TimingEepromToNode(timing_node * pio,timing_node_eeprom * pe)
{
	memcpy(pio,&(pe->node),sizeof(timing_node));
}
void TimingNodeToEeprom(timing_node * pio,timing_node_eeprom * pe)
{
	memcpy(&(pe->node),pio,sizeof(timing_node));
	pe->id = 0x55AA;
	pe->crc = 0xAA55;
}


MUTEX                    sys_timing_node_lock;
timing_node              sys_timing_node[SYS_TIMING_COUNT_MAX];
uint16_t                 sys_timing_node_count;


void dump_timing_node(timing_node * pt)
{
	if(THISINFO) {
		printf("addr[%d:%d]Start:%d-%d-%d,%d-%d-%d   ,   End:%d-%d-%d,%d-%d-%d  ,  duty:%d at %d  , ",pt->addr[1],pt->addr[0],
			pt->start_time.year+1900,pt->start_time.mon+1,pt->start_time.day,pt->start_time.hour,pt->start_time.min,pt->start_time.sec,
			pt->end_time.year+1900,pt->end_time.mon+1,pt->end_time.day,pt->end_time.hour,pt->end_time.min,pt->end_time.sec,
			(int)pt->duty_cycle,(int)pt->period);
		if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_YEAR) {
			printf("cycle year");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_MONTH) {
			printf("cycle month");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_DAY) {
			printf("cycle day");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_HOUR) {
			printf("cycle hour");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_MINITH) {
			printf("cycle minute");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_SECOND) {
			printf("cycle second");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_WEEK) {
			printf("cycle week");
		} else if(GET_IO_TIME_CYCLE_TYPE(pt) == CYCLE_ONCE) {
			printf("cycle once");
		} else {
		}
		if(GET_IO_TIME_VALID(pt)) {
			printf(",  valid  ");
		} else {
			printf(", invalid ");
		}
		if(GET_IO_TIME_SUBCYCLEON(pt)) {
			printf(",  sub cycle on  \r\n");
		} else {
			printf(",  sub cycle off \r\n");
		}
	}
}

int timing_load_form_eeprom(void)
{
	uint16_t i;
	timing_node * pn;
	timint_info   info;
	timing_node_eeprom erom;

	uint16_t count;

	if(BspReadIoTimingInfo(&info)) {
		if(THISERROR)printf("ERROR:BspReadIoTimingInfo failed!\r\n");
		return -1;
	}

	count = (info.time_max_count > SYS_TIMING_COUNT_MAX)?SYS_TIMING_COUNT_MAX:info.time_max_count;

	NutMutexLock(&sys_timing_node_lock);
	sys_timing_node_count = 0;
	NutMutexUnlock(&sys_timing_node_lock);

	for(i=0;i<SYS_TIMING_COUNT_MAX;i++) {
		pn = &sys_timing_node[i];
		NutMutexLock(&sys_timing_node_lock);
		SET_IO_TIME_VALID(pn,0);
		NutMutexUnlock(&sys_timing_node_lock);
	}

	for(i=0;i<count;i++) {
		if(BspReadIoTiming(i,&erom)) {
			if(THISERROR)printf("ERROR:BspReadIoTiming failed!\r\n");
			return -1;
		} else {
			pn = &sys_timing_node[i];
			pn->state = TIME_NO_COME; //必须初始化为未到
		    NutMutexLock(&sys_timing_node_lock);
			memcpy(pn,&(erom.node),sizeof(timing_node));
			dump_timing_node(pn);
			++sys_timing_node_count;
		    NutMutexUnlock(&sys_timing_node_lock);
		}
	}
	return 0;
}

int timing_init(void)
{
	sys_timing_node_count = 0;

	NutMutexInit(&sys_timing_node_lock);

#if 0

	for(i=0;i<SYS_TIMING_COUNT_MAX;i++) {
		pn = &sys_timing_node[i];
		NutMutexLock(&sys_timing_node_lock);
		SET_IO_TIME_VALID(pn,0);
		NutMutexUnlock(&sys_timing_node_lock);
	}


	sys_timing_node_count = 7;
	for(i=0;i<sys_timing_node_count;i++) {
		timing_node node;
		pn = &sys_timing_node[i];
		
		node.addr[1] = 0;
		node.addr[0] = i%7;
		node.start_time.year = 111;
		node.start_time.mon  = 0;
		node.start_time.day  = 1;
		node.start_time.hour = 0;
		node.start_time.min = 0;
		node.start_time.sec = 9+i;
		node.end_time.year = 111;
		node.end_time.mon  = 0;
		node.end_time.day  = 1;
		node.end_time.hour = 0;
		node.end_time.min = 0;
		node.end_time.sec = 13+i;

		node.duty_cycle = 500;
		node.period = 1000;

		node.option = 0;
		node.state = TIME_NO_COME;


		SET_IO_TIME_DONE(&node,0);
		SET_IO_TIME_VALID(&node,1);
	    SET_IO_TIME_SUBCYCLEON(&node,1);
	    SET_IO_TIME_CYCLE_TYPE(&node,CYCLE_MINITH);

		memcpy(pn,&node,sizeof(timing_node));

		dump_timing_node(pn);

	}
#else
	timing_load_form_eeprom();
#endif

	if(THISINFO)printf("Initialize timing parammeter successful(%d)!\r\n",sys_timing_node_count);
	return 0;
}




//-------------------------------------------------------------------------------

const uint32_t second_of_min_array[60] = {
    0,
    60,
    120,
    180,
    240,
    300,
    360,
    420,
    480,
    540,
    600,
    660,
    720,
    780,
    840,
    900,
    960,
    1020,
    1080,
    1140,
    1200,
    1260,
    1320,
    1380,
    1440,
    1500,
    1560,
    1620,
    1680,
    1740,
    1800,
    1860,
    1920,
    1980,
    2040,
    2100,
    2160,
    2220,
    2280,
    2340,
    2400,
    2460,
    2520,
    2580,
    2640,
    2700,
    2760,
    2820,
    2880,
    2940,
    3000,
    3060,
    3120,
    3180,
    3240,
    3300,
    3360,
    3420,
    3480,
    3540
};

const uint32_t second_of_hour_array[24] = {
    0UL,
    3600UL,
    7200UL,
    10800UL,
    14400UL,
    18000UL,
    21600UL,
    25200UL,
    28800UL,
    32400UL,
    36000UL,
    39600UL,
    43200UL,
    46800UL,
    50400UL,
    54000UL,
    57600UL,
    61200UL,
    64800UL,
    68400UL,
    72000UL,
    75600UL,
    79200UL,
    82800UL
};

const uint32_t second_of_week_array[7] = {
    0UL,
    86400UL,
    172800UL,
    259200UL,
    345600UL,
    432000UL,
    518400UL
};

uint32_t io_timing_on_msk = 0xFFFFFFFFUL;

void SetTimingOnMsk(uint32_t timing_on_msk,uint32_t timing_off_msk)
{
	io_timing_on_msk |=  timing_on_msk;
	io_timing_on_msk &= ~timing_off_msk;
}
void GetTimingOnMsk(uint32_t * timing_on_msk,uint32_t * timing_off_msk)
{
	*timing_on_msk  =  io_timing_on_msk;
	*timing_off_msk = ~io_timing_on_msk;
}

extern const uint32_t code_msk[32];

int timing_ctl_io_node(const timing_node * node,int on)
{
	unsigned char reg0x1 = 0x1;
	unsigned char reg0x0 = 0x0;
	if(node->addr[1] == 0) {
		if(node->addr[0] < 32) {
			if(code_msk[node->addr[0]]&io_timing_on_msk) {
		        if(on) {
					io_out_set_bits(node->addr[0],&reg0x1,1);
			        //SetRelayOneBitWithDelay(node->addr[0]);
		        } else {
			        //ClrRelayOneBitWithDelay(node->addr[0]);
					io_out_set_bits(node->addr[0],&reg0x0,1);
		        }
			}
		}
	} else {
		if(THISINFO)printf("timing_ctl_io_node,addr[1] != 0 ERROR!\r\n");
	}
	return 0;
}

void time_to_tm(struct _tm * ttm,const time_type * time)
{
	ttm->tm_sec = time->sec;
	ttm->tm_min = time->min;
	ttm->tm_hour = time->hour;
	ttm->tm_mday = time->day;
	ttm->tm_mon = time->mon;
	ttm->tm_year = time->year;
}


//时间比较
//时间未到，返回-1
//时间超过，返回1
//时间相等，返回0
//精确到秒钟
int time_arrived_compare(const struct _tm * now,const struct _tm * start)
{
		if(now->tm_year > start->tm_year) {
			return 0;
		} else if(now->tm_year == start->tm_year) {
			if(now->tm_mon > start->tm_mon) {
				return 0;
			} else if(now->tm_mon == start->tm_mon) {
				if(now->tm_mday > start->tm_mday) {
					return 0;
				} else if(now->tm_mday == start->tm_mday) {
					if(now->tm_hour > start->tm_hour) {
						return 0;
					} else if(now->tm_hour == start->tm_hour) {
						if(now->tm_min > start->tm_min) {
							return 0;
						} else if(now->tm_min == start->tm_min) {
							if(now->tm_sec > start->tm_sec) {
								return 1;
							} else if(now->tm_sec == start->tm_sec) {
								return 0;
							}
						}
					}
				}
			}
		}
		return -1;
}

//计算某个时间从0点,或者0分,或者每周日0点开始经过的秒数
uint32_t time_escape_abs(const struct _tm * time,uint8_t type)
{
	uint32_t esc = 0;
	ASSERT_MSG(time->tm_sec < 60,("time->tm_sec < 60 assert failed!\r\n"));
	switch(type)
	{
	case CYCLE_SECOND:
		return 0;
	case CYCLE_MINITH:
		return time->tm_sec;
	case CYCLE_HOUR:
	    ASSERT_MSG(time->tm_min < 60,("time->tm_min < 60 assert failed!\r\n"));
		esc = time->tm_sec;
		esc += second_of_min_array[time->tm_min]; // * 60;
		return esc;
	case CYCLE_DAY:
	    ASSERT_MSG(time->tm_min < 60,("time->tm_min < 60 assert failed!\r\n"));
	    ASSERT_MSG(time->tm_hour < 24,("time->tm_min < 24 assert failed!\r\n"));
		esc = time->tm_sec;
		esc += second_of_min_array[time->tm_min]; // * 60;
		esc += second_of_hour_array[time->tm_hour];  // * 3600;
		return esc;
	case CYCLE_WEEK:
	    ASSERT_MSG(time->tm_min < 60,("time->tm_min < 60 assert failed!\r\n"));
	    ASSERT_MSG(time->tm_hour < 24,("time->tm_min < 24 assert failed!\r\n"));
		ASSERT_MSG(time->tm_mday < 7,("time->week < 7 assert failed!\r\n"));
		esc = time->tm_sec;
		esc += second_of_min_array[time->tm_min]; // * 60;
		esc += second_of_hour_array[time->tm_hour];  // * 3600;
		esc += second_of_week_array[time->tm_mday]; // * 24*3600;
		return esc;
	case CYCLE_MONTH:
	case CYCLE_YEAR:
		return -1;
	case CYCLE_ONCE:
	default:
		break;
	}
	return -1;
}
#define  ONE_DAY_MAX_SECS   (24UL*3600UL)
#define  ONE_HOUR_MAX_SECS  (3600UL)
#define  ONE_MIN_MAX_SECS   (60)
#define  ONE_WEEK_MAX_SECS  (7*24*3600UL)
uint32_t time_comapre(const uint32_t time,const uint32_t ref,uint8_t type)
{
	uint32_t esc = 0;
	switch(type)
	{
	case CYCLE_SECOND:
		return 0;
	case CYCLE_MINITH:
		if(time < ref) {
			return (time+ONE_MIN_MAX_SECS-ref);
		} else {
			return (time-ref);
		}
	case CYCLE_HOUR:
		if(time < ref) {
			return (time+ONE_HOUR_MAX_SECS-ref);
		} else {
			return (time-ref);
		}
		return esc;
	case CYCLE_DAY:
		if(time < ref) {
			return (time+ONE_DAY_MAX_SECS-ref);
		} else {
			return (time-ref);
		}
		return esc;
	case CYCLE_WEEK:
		if(time < ref) {
			return (time+ONE_WEEK_MAX_SECS-ref);
		} else {
			return (time-ref);
		}
		return esc;
	case CYCLE_MONTH:
	case CYCLE_YEAR:
	case CYCLE_ONCE:
	default:
		break;
	}
	return -1;
}
//计算当前时间跟某个时间相比，经过了多少秒
//
int time_escape_how_seconds(const struct _tm * sys_now,const struct _tm * start,uint8_t type,uint32_t * escape_time)
{
	struct _tm _now;
	struct _tm * now = &_now;
	memcpy(now,sys_now,sizeof(struct _tm));
	*escape_time = 0;
	switch(type)
	{
	case CYCLE_SECOND:
		return 0; //每秒都是周期
	case CYCLE_MINITH:
	case CYCLE_HOUR:
	case CYCLE_DAY:
	case CYCLE_WEEK:
		{
			uint32_t now_sec = time_escape_abs(now,type);
			uint32_t tick_sec = time_escape_abs(start,type);
			*escape_time = time_comapre(now_sec,tick_sec,type);
			return 0;
		}
	case CYCLE_ONCE:
		return time_arrived_compare(now,start);
	case CYCLE_USER:
	default:
		return -1;
	}
	return -1;
}

//计算时间是否到了，或者是否过了
int  now_is_on_io_timing(const struct _tm * sys_now,const timing_node * time,uint32_t * on_time)
{
	int ret = -1;
	struct _tm nowv;
	struct _tm * now = &nowv;
	struct _tm time_start,time_end;

	memcpy(now,sys_now,sizeof(struct _tm));
	time_to_tm(&time_start,&(time->start_time));
	time_to_tm(&time_end,&(time->end_time));

	*on_time = 0;

	switch(GET_IO_TIME_CYCLE_TYPE(time))
	{
	case CYCLE_WEEK:
		{
			uint32_t ends;
			//把现在的时间，星期参数加进去
			now->tm_mday = sys_time_week; //get_week_day(sys_now->tm_year+1900,sys_now->tm_mon+1,sys_now->tm_mday);
			if(0)printf("now week is %d\r\n",now->tm_mday);
			ret = time_escape_how_seconds(now,&time_start,CYCLE_WEEK,on_time);
		    ret = time_escape_how_seconds(now,&time_end,CYCLE_WEEK,&ends);
			if(0&&THISINFO) {
				uint32_t dis = ends - *on_time;
				printf("OnTimeS:0x%X%X,",(unsigned int)(*on_time>>16),(unsigned int)(*on_time&0xFFFF));
				printf("Ends:0x%X%X,",(unsigned int)(ends>>16),(unsigned int)(ends&0xFFFF));
				printf("TimeDissS:0x%X%X\r\n",(unsigned int)(dis>>16),(unsigned int)(dis&0xFFFF));
			}
			if(*on_time < ends) {
				return ret;
			} else {
				return 1;
			}
		}
		break;
	case CYCLE_ONCE:
		{
			int ret2;
			uint32_t ends;
			ret = time_escape_how_seconds(now,&time_start,CYCLE_ONCE,on_time);
		    ret2 = time_escape_how_seconds(now,&time_end,CYCLE_ONCE,&ends);
			if(ret2 >= 0) {
				return  1;
			} else if(ret >= 0) {
				return 0;
			}
			return -1;
		}
		break;
	case CYCLE_USER: return -1;
	case CYCLE_YEAR: return -1;
	case CYCLE_MONTH:return -1;
	case CYCLE_DAY:
		{
			uint32_t ends;
			ret = time_escape_how_seconds(now,&time_start,CYCLE_DAY,on_time);
		    ret = time_escape_how_seconds(now,&time_end,CYCLE_DAY,&ends);
			if(*on_time < ends) {
				//if(THISINFO)printf("start_time=%d",*on_time);
				return ret;
			} else {
				return 1;
			}
		}
		break;
	case CYCLE_HOUR:
		{
			uint32_t ends;
			ret = time_escape_how_seconds(now,&time_start,CYCLE_HOUR,on_time);
		    ret = time_escape_how_seconds(now,&time_end,CYCLE_HOUR,&ends);
			if(*on_time < ends) {
				return ret;
			} else {
				return 1;
			}
		}
		break;
	case CYCLE_MINITH:
		{
			uint32_t ends;
			ret = time_escape_how_seconds(now,&time_start,CYCLE_MINITH,on_time);
		    ret = time_escape_how_seconds(now,&time_end,CYCLE_MINITH,&ends);
			if(*on_time < ends) {
				return ret;
			} else {
				return 1;
			}
		}
		break;
	case CYCLE_SECOND:
		{
			return 0;
		}
		break;
	default:
		break;
	}
	return ret;
}

int  now_is_on_io_sub_timing(const struct _tm * now,const uint32_t now_ms,timing_node * node)
{
	uint32_t on_time;
	int pos;
	int type = GET_IO_TIME_CYCLE_TYPE(node); //获取定时的循环类型
	if(!GET_IO_TIME_VALID(node)) { //如果此定时节点无效，则直接退出，不再分析
		return -1;
	}
	pos = now_is_on_io_timing(now,node,&on_time); //计算pos值，返回-1,0,1三个结果。
	if(pos < 0) { //在定时时间之前，时间还没到，不用动作
	} else if(pos > 0) { //定时完毕，关闭，时间已经过去了，判断几种情况
		if(node->state == TIME_NO_COME) { //节点的状态表明，这个定时没有操作过。
			//还未定时操作过,不要动作
			return -1;
		} else if(node->state == TIME_DOING || node->state == TIME_SUBDO_ON) { //定时正在操作之中，或者
			//定时刚刚完毕,动作关闭
			timing_ctl_io_node(node,0);
			node->state = TIME_OVER;
			SET_IO_TIME_DONE(node,1);
			return 1;
		}
	} else {//在定时器内,pos == 0的情况，说明现在的时间(now)，刚好落在定时的时间范围内。
		//更新状态
		if((node->state == TIME_OVER || node->state == TIME_NO_COME)) {
			node->state = TIME_DOING;
		}
		if(node->state == TIME_DOING || node->state == TIME_SUBDO_ON || node->state == TIME_SUBDO_OFF) {
			//if(THISINFO)printf("in timing on...");
          if(GET_IO_TIME_SUBCYCLEON(node) && node->period > 0) { //有循环的
			  //if(THISINFO)printf("have sub cycle...%d,%d",(int)node->duty_cycle,(int)node->period);
			  if(type == CYCLE_MONTH || type == CYCLE_DAY || type == CYCLE_HOUR || 
				 type == CYCLE_MINITH || type == CYCLE_SECOND || type == CYCLE_WEEK) 
			  { //循环类型符合要求
				uint32_t duty = (on_time*1000+now_ms) % node->period;
				//if(THISINFO)printf("cycle type ok,duty(%d)...",(int)duty);
				//计算是否在占空比内
				    if(duty < node->duty_cycle) {
		                //在占空比内，打开
						//if(THISINFO)printf(" on duty sub cycle...\r\n");
						if((node->state == TIME_DOING) || (node->state == TIME_SUBDO_OFF)) {
						    node->state = TIME_SUBDO_ON;
		                    timing_ctl_io_node(node,1);
						}
	                } else {
		                //关闭
					    //if(THISINFO)printf(" out duty sub cycle...\r\n");
						if((node->state == TIME_DOING) || (node->state == TIME_SUBDO_ON)) {
							node->state = TIME_SUBDO_OFF;
		                    timing_ctl_io_node(node,0);
						}
		            }
			  } else if(type == CYCLE_ONCE) {
				  if((node->state == TIME_DOING) || (node->state == TIME_SUBDO_OFF)) {
				      node->state = TIME_SUBDO_ON;
				      timing_ctl_io_node(node,1);
				  }
			  } else {//循环类型错误
				  if(THISERROR)printf("ERROR:now_is_on_io_sub_timing sub cycle type is ERROR! 1 LEVEL\r\n");
			  }
		  } else {//没有循环的
		    //直接打开好了
			 // if(THISINFO)printf(" no sub cycle...");
		    if(node->state == TIME_DOING) {
				node->state = TIME_SUBDO_ON;
			    timing_ctl_io_node(node,1);
			} else {
			}
		  }
		}
	}
	return 0;
}



void io_scan_timing_server(void)
{
	//定时控制
	uint16_t i;
	for(i=0;i<sys_timing_node_count;i++) {
		now_time now;
		now = get_system_time();
		NutMutexLock(&sys_timing_node_lock);
		if(now_is_on_io_sub_timing(&now.sys_time,now.sys_time_ms,&sys_timing_node[i]) == 1) {
			//保存回EEPROM
		}
		NutMutexUnlock(&sys_timing_node_lock);
	}
}


/*************************************************/
/* 新的定时控制器模块 */
/* 清空，添加，删除，更新指定index的定时，TCP2000地址  */
/* 日循环，周循环，24小时内执行 */
/*************************************************/
