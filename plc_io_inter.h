#ifndef __PLC_IO_INTERFACE_H__
#define __PLC_IO_INTERFACE_H__

#define   HOLDER_REGISTER_BYTES        32  //56

extern unsigned int phy_io_in_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_get_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern unsigned int phy_io_out_set_bits(unsigned int startbits,unsigned char * iobits,unsigned int bitcount);
extern void holder_register_read(unsigned int start,unsigned char * buffer,unsigned int len);
extern void holder_register_write(unsigned int start,unsigned char * buffer,unsigned int len);
extern unsigned int load_plc_form_eeprom(unsigned int start,unsigned char * buffer,unsigned int len);
extern unsigned int write_plc_to_eeprom(unsigned int start,unsigned char * buffer,unsigned int len);

/*****************************
 *   提供标准的接口
 */
#define  POWERUP_RESET       0x00
#define  WDT_RESET           0x01
#define  EXT_RESET           0x02
#define  SOFT_RESET          0x03
extern unsigned char get_reset_type(void);

#endif
