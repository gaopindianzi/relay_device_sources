#ifndef __PLC_IO_INTERFACE_H__
#define __PLC_IO_INTERFACE_H__



extern unsigned int phy_io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);


#endif
