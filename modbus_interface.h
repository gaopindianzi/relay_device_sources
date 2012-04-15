#ifndef __MOSBUS_INTERFACE_H__
#define __MOSBUS_INTERFACE_H__


typedef struct _modbus_tcp_head
{
	uint8_t idh;
	uint8_t idl;
	uint8_t protocolh;
	uint8_t protocoll;
	uint8_t lengthh;
	uint8_t lengthl;
	uint8_t slave_addr;
	uint8_t function_code;
} modbus_tcp_head;




void StartModbus_Interface(void);

#endif
