#ifndef __SYS_VAR_H__
#define __SYS_VAR_H__


typedef struct _sys_varient_type
{
	FILE  * iofile;
#ifdef APP_485_ON
	FILE  * stream_max485;
#endif
	FILE  * resetfile;
} sys_varient_type;

extern sys_varient_type sys_varient;

#endif
