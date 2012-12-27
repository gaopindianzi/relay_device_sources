#ifndef __PLC_IO_INTERFACE_H__
#define __PLC_IO_INTERFACE_H__

#define   HOLDER_REGISTER_BYTES        56

extern unsigned int phy_io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern void holder_register_read(unsigned int start,unsigned char * buffer,unsigned int len);
extern void holder_register_write(unsigned int start,unsigned char * buffer,unsigned int len);


#endif
