#ifndef __SYS_VAR_H__
#define __SYS_VAR_H__


typedef struct _sys_varient_type
{
	FILE  * iofile;
	FILE  * stream_max485;
	FILE  * resetfile;
} sys_varient_type;

extern sys_varient_type sys_varient;

#endif
