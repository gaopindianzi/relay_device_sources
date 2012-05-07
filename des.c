
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

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bsp.h"
#include "sysdef.h"

#include "des.h"


#include "debug.h"

#define THISINFO        0
#define THISASSERT      1


static const unsigned char PROGMEM DesIp[] =
{ 
	58, 50, 42, 34, 26, 18, 10, 2,
	60, 52, 44, 36, 28, 20, 12, 4,
	62, 54, 46, 38, 30, 22, 14, 6,
	64, 56, 48, 40, 32, 24, 16, 8,
	57, 49, 41, 33, 25, 17, 9,   1,
	59, 51, 43, 35, 27, 19, 11, 3,
	61, 53, 45, 37, 29, 21, 13, 5,
	63, 55, 47, 39, 31, 23, 15, 7
};

//pgm_read_byte

static const unsigned char PROGMEM DesIp_1[] =
{
	40, 8,  48, 16, 56, 24, 64, 32,
	39, 7,  47, 15, 55, 23, 63, 31,
	38, 6,  46, 14, 54, 22, 62, 30,
	37, 5,  45, 13, 53, 21, 61, 29,
	36, 4,  44, 12, 52, 20, 60, 28,
	35, 3,  43, 11, 51, 19, 59, 27,
	34, 2,  42, 10, 50, 18, 58, 26,
	33, 1,  41, 9,  49, 17, 57, 25
};


static const unsigned char PROGMEM DesS[8][4][16] = 
{
	{
		{14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7},
		{0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8},
		{4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0},
		{15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13}
	},
	
	{
		{15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10},
		{3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5},
		{0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15},
		{13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9}
	},
	
	{
		{10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8},
		{13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1},
		{13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7},
		{1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12}
	},
	
	{
		{7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15},
		{13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9},
		{10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4},
		{3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14}
	},
	
	{
		{2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9},
		{14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6},
		{4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14},
		{11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3}
	},
	
	{
		{12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11},
		{10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8},
		{9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6},
		{4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13}
	},
	
	{
		{4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1},
		{13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6},
		{1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2},
		{6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12}
	},
	
	{
		{13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7},
		{1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2},
		{7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8},
		{2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}
	}
};



static const unsigned char PROGMEM DesE[] =
{
	32, 1, 2, 3, 4, 5,
	4, 5, 6, 7, 8, 9,
	8, 9, 10, 11, 12,13,
	12, 13, 14, 15, 16, 17,
	16, 17, 18, 19, 20, 21,
	20, 21, 22, 23, 24, 25,
	24, 25, 26, 27, 28, 29,
	28, 29, 30, 31, 32, 1
};



static const unsigned char PROGMEM DesP[] = 
{
	16, 7, 20, 21,
	29, 12, 28, 17,
	1, 15, 23, 26,
	5, 18, 31, 10,
	2, 8, 24,  14,
	32, 27, 3, 9,
	19, 13, 30, 6,
	22, 11, 4, 25
};



static const unsigned char PROGMEM DesPc_1[] =
{
	57, 49, 41, 33, 25, 17, 9,
	1, 58, 50, 42, 34, 26, 18,
	10, 2, 59, 51, 43, 35, 27,
	19, 11, 3, 60, 52, 44, 36,
	63, 55, 47, 39, 31, 23, 15,
	7, 62, 54, 46, 38, 30, 22,
	14, 6, 61, 53, 45, 37, 29,
	21, 13, 5, 28, 20, 12, 4
};



static const unsigned char PROGMEM DesPc_2[] =
{
	14, 17, 11, 24, 1, 5,
	3, 28, 15, 6, 21, 10,
	23, 19, 12, 4, 26, 8,
	16, 7, 27, 20, 13, 2,
	41, 52, 31, 37, 47, 55,
	30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53,
	46, 42, 50, 36, 29, 32
};



static const unsigned char PROGMEM DesRots[] = 
{
	1, 1, 2, 2, 
	2, 2, 2, 2, 
	1, 2, 2, 2, 
	2, 2, 2, 1, 
	0
};




static void movram(unsigned char* source,char type,unsigned char* target,char type2,unsigned char length);
static void doxor(unsigned char* sourceaddr,char type1,unsigned char* targetaddr,char type2,unsigned char length);
static void setbit(unsigned char* dataddr,char type,unsigned char pos,unsigned char b0)	;
static unsigned char getbit(unsigned char* dataddr,char type,unsigned char pos)	;
static void selectbits(unsigned char* source,char type1,unsigned char* table,char type2,unsigned char* target,char type3,unsigned char count);
static void shlc(unsigned char* data_p,char type);
static void shrc(unsigned char* data_p,char type);
static void strans(unsigned char* index,char type1,unsigned char* target,char type2);

#define  SRAM      0
#define  PGROM     1

static void movram(unsigned char* source,char type,unsigned char* target,char type2,unsigned char length)
{
	unsigned char i;

	ASSERT(type2!=PGROM);
	
	for(i = 0;i < length;i++)/*?????*/
	{
		target[i] = (type==PGROM)?pgm_read_byte(&source[i]):source[i];
	}
}



static void doxor(unsigned char* sourceaddr,char type1,unsigned char* targetaddr,char type2,unsigned char length)
{
	unsigned char i;
	
	ASSERT(type1!=PGROM);

	for (i = 0;i < length;i++)/*???*/
	{
		sourceaddr[i] ^= (type2==PGROM)?pgm_read_byte(&targetaddr[i]):targetaddr[i];
	}
}


static void setbit(unsigned char* dataddr,char type,unsigned char pos,unsigned char b0)	
{
	unsigned char byte_count;
	unsigned char bit_count;
	unsigned char temp;

	ASSERT(type!=PGROM);

	temp = 1;
	byte_count = (pos - 1) / 8;
	bit_count = 7 - ((pos - 1) % 8);
	temp <<= bit_count;
	
	if(b0)
	{
		dataddr[byte_count] |= temp;
	}
	else
	{
		temp = ~temp;
		dataddr[byte_count] &= temp;
	}
}



static unsigned char getbit(unsigned char* dataddr,char type,unsigned char pos)	
{
	unsigned char byte_count;
	unsigned char bit_count;
	unsigned char temp;

	temp = 1;
	byte_count = (pos - 1) / 8;
	bit_count = 7 - ((pos - 1) % 8);
	temp <<= bit_count;
	if(((type==PGROM)?pgm_read_byte(&dataddr[byte_count]):dataddr[byte_count]) & temp)
	{
		return(1);
	}
	else 
	{
		return(0);
	}
}



static void selectbits(unsigned char* source,char type1,unsigned char* table,char type2,unsigned char* target,char type3,unsigned char count)
{
	unsigned char i;

	ASSERT(type3!=PGROM);
	
	for(i = 0;i < count;i++)
	{
		setbit(target,type3,i + 1,getbit(source,type1,((type2==PGROM)?pgm_read_byte(&table[i]):table[i]))); 
	}
}



static void shlc(unsigned char* data_p,char type)
{
	unsigned char i,b0;

	ASSERT(type!=PGROM);
	
	b0 = getbit(data_p,type,1);
	
	for(i = 0;i < 7;i++)
	{
		data_p[i] <<= 1;
		
		if(i != 6)
		{
			setbit(&data_p[i],type,8,getbit(&data_p[i + 1],type,1));
		}
	}
	
	setbit(data_p,type,56,getbit(data_p,type,28));
	setbit(data_p,type,28,b0);
}


static void shrc(unsigned char* data_p,char type)
{
	unsigned char b0;
	int i;

	ASSERT(type!=PGROM);
	
	b0 = getbit(data_p,type,56);
	
	for(i = 6;i >= 0;i--)
	{
		data_p[i] >>= 1;
		
		if(i != 0)
		{
			setbit(&data_p[i],type,1,getbit(&data_p[i - 1],type,8)); 
		}
	}
	
	setbit(data_p,type,1,getbit(data_p,type,29));
	setbit(data_p,type,29,b0);
}



/* The problem is about yielded in this function */
static void strans(unsigned char* index,char type1,unsigned char* target,char type2)
{
	unsigned char row,line,t,i,j,b0,b1;

	ASSERT(type2!=PGROM);
	
	for(i = 0;i < 4;i++)
	{
		row = line = t = 0;
		setbit(&line,SRAM,7,b0 = getbit(index,type1,i * 12 + 1));
		setbit(&line,SRAM,8,b1 = getbit(index,type1,i * 12 + 6));
		
		for(j = 2;j < 6;j++)
		{
			setbit(&row,SRAM,3 + j,getbit(index,type1,i * 12 + j));
		}
		
		t = pgm_read_byte(&DesS[i * 2][line][row]);
		t <<= 4;
		line = row = 0; 
		setbit(&line,SRAM,7,getbit(index,type1,i * 12 + 7));
		setbit(&line,SRAM,8,getbit(index,type1,i * 12 + 12));
		
		for(j = 2;j < 6;j++)
		{
			setbit(&row,SRAM,3 + j,getbit(index,type1,i * 12 + 6 + j));
		}
		
		t |= pgm_read_byte(&DesS[i * 2 + 1][line][row]);
		target[i] = t;
	}
}




void des(unsigned char *data_p,char type1,unsigned char* key_p,char type2,int type)
{

	unsigned char tempbuf[12];
	unsigned char key[7];
	unsigned char i;
	unsigned char j;
	unsigned char count;
	void (*f)(unsigned char* data_p,char type);

	selectbits(data_p,type1,(unsigned  char *)DesIp,PGROM,tempbuf,SRAM,64);/*????*/

	movram(tempbuf,SRAM,data_p,type1,8);
	selectbits((unsigned  char *)key_p,type2,(unsigned  char *)DesPc_1,PGROM,(unsigned  char *)key,SRAM,56);/*KEY?????*/
	
	for(i = 0;i < 16;i ++)
	{
		selectbits((unsigned  char *)data_p + 4,type1,(unsigned  char *)DesE,PGROM,tempbuf,SRAM,48);/*????*/
		
		if(type ==1)		//jia mi
		{
			f = shlc;
			count = i;
		}
		else
		{
			count = 16 - i;	
			f = shrc;
		}
		
		for(j = 0;j < pgm_read_byte(&DesRots[count]);j++)/*KEY ???*/
		{
			f(key,SRAM);
		}
		
		selectbits(key,SRAM,(unsigned char *)DesPc_2,PGROM,tempbuf + 6,SRAM,48);/*KEY ????*/
		doxor(tempbuf,SRAM,tempbuf + 6,SRAM,6);
		strans(tempbuf,SRAM,tempbuf + 6,SRAM);
		selectbits(tempbuf + 6,SRAM,(unsigned char *)DesP,PGROM,tempbuf,SRAM,32);
		doxor(tempbuf,SRAM,data_p,type1,4);
		
		if(i < 15)
		{
			movram(data_p + 4,type1,data_p,type1,4);
			movram(tempbuf,SRAM,data_p + 4,type1,4);
		}
	}
	
	movram(tempbuf,SRAM,data_p,type1,4);
	selectbits(data_p,type1,(unsigned char *)DesIp_1,PGROM,tempbuf,SRAM,64);
	movram(tempbuf,SRAM,data_p,type1,8);

}
