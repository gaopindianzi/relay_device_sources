#ifndef ___EEPROM_USE_MAP__H___
#define ___EEPROM_USE_MAP__H___

#include "bsp.h"

#define  SYS_USE_SIZE        BSP_MAX_OFFSET
#define  PLC_FLASH_SIZE      (4096 - SYS_USE_SIZE)

typedef struct __eeprom_map_t
{
	unsigned char sys_use[SYS_USE_SIZE];
	unsigned char plc_flash[PLC_FLASH_SIZE];
} eeprom_map_t;


#endif

