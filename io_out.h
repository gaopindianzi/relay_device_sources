#ifndef __IO_OUT_H__
#define __IO_OUT_H__


void InitTimingLock(void);
int TimingIoLoadToRam(void);

/*
void SetRelayOneBitWithDelay(unsigned char index);
void ClrRelayOneBitWithDelay(unsigned char index);
void SetInputOnMsk(uint32_t input_on_msk,uint32_t input_off_msk);
void GetInputOnMsk(uint32_t * input_on_msk,uint32_t * input_off_msk);
*/

unsigned int io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
unsigned int io_out_convert_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);


#endif
