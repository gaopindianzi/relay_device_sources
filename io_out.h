#ifndef __IO_OUT_H__
#define __IO_OUT_H__


void InitTimingLock(void);
int TimingIoLoadToRam(void);

typedef struct _input_trigger_mode
{
	unsigned char mode[INPUT_CHANNEL_NUM];
} input_trigger_mode;

unsigned int io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_convert_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);


#endif
