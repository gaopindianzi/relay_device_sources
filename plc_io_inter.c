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
#include <time.h>
#include <sys/mutex.h>
#include <sys/atom.h>
#include <dev/ds1307rtc.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "sysdef.h"
#include "bin_command_def.h"
#include "bsp.h"
#include "time_handle.h"
#include "io_time_ctl.h"
#include <dev/reset_avr.h>
#include <dev/relaycontrol.h>
#include "sys_var.h"
#include "io_out.h"
#include "debug.h"

#include "plc_command_def.h"
#include "plc_prase.h"
#include "compiler.h"
#include "eeprom_use_map.h"

#define THISINFO  1
#define THISERROR 1

extern unsigned char io_out[32/8];


unsigned int phy_io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	unsigned char buffer[2];
	//uint32_t tmp;

	memset(iobits,0,(bitcount+7)/8);

	//_ioctl(_fileno(sys_varient.iofile), GET_IN_NUM, &tmp);
	//参数必须符合条件
	if(startbits >= INPUT_CHANNEL_NUM || bitcount == 0) {
		//printf("io_in_get bit param error1:startbits = %d , bit count = %d\r\n",startbits,bitcount);
		return 0;
	}
	//进一步判断是否符合条件
	if((INPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = INPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_in_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//开始设置
	index = 0;
	//
	_ioctl(_fileno(sys_varient.iofile), IO_IN_GET, buffer);
	
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(buffer[i/8]&code_msk[i%8]) {
			iobits[Bb] |=  code_msk[Bi];
		} else {
			iobits[Bb] &= ~code_msk[Bi];
		}
		index++;
	}
	return bitcount;
}

unsigned int phy_io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;

	memset(iobits,0,(bitcount+7)/8);

	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//参数必须符合条件

	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//printf("io_out_get_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	//开始设置
	index = 0;
	
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(io_out[i/8]&code_msk[i%8]) {
			iobits[Bb] |=  code_msk[Bi];
		} else {
			iobits[Bb] &= ~code_msk[Bi];
		}
		index++;
	}
	return bitcount;
}


extern unsigned char switch_signal_hold_time[32];
extern unsigned char io_out[32/8];
extern unsigned char io_input_on_msk[32/8];
extern unsigned char io_pluase_time_count;


unsigned int phy_io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount)
{
	unsigned int i,index;
	unsigned char Bb,Bi;
	//uint32_t tmp;
	//_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	//参数必须符合条件
	if(startbits >= OUTPUT_CHANNEL_NUM || bitcount == 0) {
		return 0;
	}
	//进一步判断是否符合条件
	if((OUTPUT_CHANNEL_NUM - startbits) < bitcount) {
		bitcount = OUTPUT_CHANNEL_NUM - startbits;
	}
	//开始设置
	//printf("io_out_set_bits startbits = %d , bit count = %d\r\n",startbits,bitcount);
	index = 0;
	for(i=startbits;i<startbits+bitcount;i++) {
	    Bb = index / 8;
	    Bi = index % 8;
		if(iobits[Bb]&code_msk[Bi]) {
			io_out[i/8] |=  code_msk[i%8];
			switch_signal_hold_time[i] = io_pluase_time_count;
		} else {
			io_out[i/8] &= ~code_msk[i%8];
			switch_signal_hold_time[i] = 0;
		}
		index++;
	}
	NutEventPost(&(sys_varient.io_out_event));
	return bitcount;
}

void holder_register_read(unsigned int start,unsigned char * buffer,unsigned int len)
{
	DS1307RamRead(0x08+start,buffer,len);
}

void holder_register_write(unsigned int start,unsigned char * buffer,unsigned int len)
{
	DS1307RamWrite(0x08+start,buffer,len);
}


#define  POWERUP_RESET       0x00
#define  WDT_RESET           0x01
#define  EXT_RESET           0x02
#define  SOFT_RESET          0x03




extern uint8_t  reset_type;  //单片机内部的数据

unsigned char get_reset_type(void)
{
	if(reset_type&AVR_EXT_RESET) {
		return EXT_RESET;
	} else if(reset_type&AVR_WDT_RESET) {
		return WDT_RESET;
	} else if(reset_type&AVR_POWER_UP_RESET) {
		return POWERUP_RESET;
	}
	return 0;
}


unsigned int load_plc_form_eeprom(unsigned int start,unsigned char * buffer,unsigned int len)
{
	if(len == 0) {
		return 0;
	}
	if(start >= GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0])) {
		return 0;
	}
	if((start + len) > GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0])) {
		len -= (start + len) - GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0]);
	}
	start += GET_OFFSET_MEM_OF_STRUCT(eeprom_map_t,plc_flash[0]); //加上偏移地址得到真实地址
	NutNvMemLoad(start,buffer,len);
	return len;
}

unsigned int write_plc_to_eeprom(unsigned int start,const unsigned char * buffer,unsigned int len)
{
	if(len == 0) {
		return 0;
	}
	if(start >= GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0])) {
		return 0;
	}
	if((start + len) > GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0])) {
		len -= (start + len) - GET_MEM_SIZE_OF_STRUCT(eeprom_map_t,plc_flash[0]);
	}
	start += GET_OFFSET_MEM_OF_STRUCT(eeprom_map_t,plc_flash[0]); //加上偏移地址得到真实地址
	NutNvMemSave(start,buffer,len);
	return len;
}


