#ifndef __IO_TIME_CTL_H__
#define __IO_TIME_CTL_H__

#ifdef APP_TIMEIMG_ON

typedef struct _timing_node_save_type
{
	uint8_t    addr[2];
	uint16_t   option;
	uint32_t   start_time;
	uint32_t   end_time;
	//定时内循环
	uint32_t   duty_cycle;
	uint32_t   period;
} timing_node_save_type;

typedef struct _timing_node_eeprom
{
	uint16_t                id;
	timing_node             node;
	uint16_t                crc;
} timing_node_eeprom;


int timing_init(void);
void TimingEepromToNode(timing_node * pio,timing_node_eeprom * pe);
void TimingNodeToEeprom(timing_node * pio,timing_node_eeprom * pe);
int timing_load_form_eeprom(void);

void SetTimingOnMsk(uint32_t timing_on_msk,uint32_t timing_off_msk);
void GetTimingOnMsk(uint32_t * timing_on_msk,uint32_t * timing_off_msk);



int BspReadIoTimingInfo(timint_info * info);
int BspReadIoTiming(uint16_t index,timing_node_eeprom * time);
int BspTimingStartWrite(void);
int BspWriteIoTimingInfo(timint_info * info);
int BspWriteIoTiming(uint16_t index,timing_node_eeprom * time);
int BspTimingDoneWrite(void);

#endif

#endif
