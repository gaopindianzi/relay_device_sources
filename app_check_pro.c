
#include <compiler.h>
#include <cfg/os.h>

//#include "bsp.h"


#if 0

#define THISINFO   0


#if 1

#if 1
prog_char app_check_data[256] = {0x0,
1,	2,	3,	4,	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,	18,	19,	20,	21,	22,	23,	24,	25,	26,	27,	28,	29,	30,	31,	
32,	33,	34,	35,	36,	37,	38,	39,	40,	41,	42,	43,	44,	45,	46,	47,	48,	49,	50,	51,	52,	53,	54,	55,	56,	57,	58,	59,	60,	61,	62,	63,	
64,	65,	66,	67,	68,	69,	70,	71,	72,	73,	74,	75,	76,	77,	78,	79,	80,	81,	82,	83,	84,	85,	86,	87,	88,	89,	90,	91,	92,	93,	94,	95,	
96,	97,	98,	99,	100,	101,	102,	103,	104,	105,	106,	107,	108,	109,	110,	111,	112,	113,	
114,	115,	116,	117,	118,	119,	120,	121,	122,	123,	124,	125,	126,	127,	128,	129,	
130,	131,	132,	133,	134,	135,	136,	137,	138,	139,	140,	141,	142,	143,	144,	145,	
146,	147,	148,	149,	150,	151,	152,	153,	154,	155,	156,	157,	158,	159,	160,	161,	
162,	163,	164,	165,	166,	167,	168,	169,	170,	171,	172,	173,	174,	175,	176,	177,	
178,	179,	180,	181,	182,	183,	184,	185,	186,	187,	188,	189,	190,	191,	192,	193,	
194,	195,	196,	197,	198,	199,	200,	201,	202,	203,	204,	205,	206,	207,	208,	209,	
210,	211,	212,	213,	214,	215,	216,	217,	218,	219,	220,	221,	222,	223,	224,	225,	
226,	227,	228,	229,	230,	231,	232,	233,	234,	235,	236,	237,	238,	239,	240,	241,	
242,	243,	244,	245,	246,	247,	248,	249,	250,	251,	252,	253, 0x55,   0xAA};
#else
prog_char app_check_data[256] = {255,	254,	253,	252,	251,	250,	249,	248,	247,	246,	245,	244,	
243,	242,	241,	240,	239,	238,	237,	236,	235,	234,	233,	232,	231,	230,	229,	
228,	227,	226,	225,	224,	223,	222,	221,	220,	219,	218,	217,	216,	215,	214,	
213,	212,	211,	210,	209,	208,	207,	206,	205,	204,	203,	202,	201,	200,	199,	
198,	197,	196,	195,	194,	193,	192,	191,	190,	189,	188,	187,	186,	185,	184,	
183,	182,	181,	180,	179,	178,	177,	176,	175,	174,	173,	172,	171,	170,	169,	
168,	167,	166,	165,	164,	163,	162,	161,	160,	159,	158,	157,	156,	155,	154,	
153,	152,	151,	150,	149,	148,	147,	146,	145,	144,	143,	142,	141,	140,	139,	
138,	137,	136,	135,	134,	133,	132,	131,	130,	129,	128,	127,	126,	125,	124,	
123,	122,	121,	120,	119,	118,	117,	116,	115,	114,	113,	112,	111,	110,	109,	
108,	107,	106,	105,	104,	103,	102,	101,	100,	99,	98,	97,	96,	95,	94,	93,	92,	91,	90,	89,	88,
87,	86,	85,	84,	83,	82,	81,	80,	79,	78,	77,	76,	75,	74,	73,	72,	71,	70,	69,	68,	67,	66,	65,	64,	63,	62,	61,	60,	59,	58,
57,	56,	55,	54,	53,	52,	51,	50,	49,	48,	47,	46,	45,	44,	43,	42,	41,	40,	39,	38,	37,	36,	35,	34,	33,	32,	31,	30,	29,	28,	
27,	26,	25,	24,	23,	22,	21,	20,	19,	18,	17,	16,	15,	14,	13,	12,	11,	10,	9,	8,	7,	6,	5,	4,	3,	2,0x55,   0xAA};
#endif

#endif


volatile unsigned char error_flag       __attribute__ ((section (".noinit")));  
volatile unsigned char addr_hig         __attribute__ ((section (".noinit")));
volatile unsigned char addr_low         __attribute__ ((section (".noinit")));
static unsigned char check_data[256]    __attribute__ ((section (".noinit")));

uint16_t Cal_CRC16(uint16_t * buffer,uint16_t len);

void Load_check_dada(void)
{
	uint16_t tmp;
	uint16_t i;
	uint8_t  * pbuffer = NULL;
	tmp = (((unsigned int)app_check_data)&0xFF00);
	tmp |= addr_low;
	pbuffer = (uint16_t *)tmp;
	if(THISINFO)printf("check_data:\r\n");
	for(i=0;i<sizeof(check_data);i++) {
		check_data[i] = pgm_read_byte(pbuffer+i);
		if(THISINFO)printf("%02X ",check_data[i]);
	}
	if(THISINFO)printf("\r\n");
}
#if 0
void check_get_mac_address(unsigned char * mac)
{
	unsigned char buf[6],crc[6],i;
	uint16_t tmp;
	tmp = Cal_CRC16(check_data,10);
	crc[0] = tmp >> 8;
	crc[1] = tmp & 0xFF;
	tmp = Cal_CRC16(&check_data[10],10);
	crc[2] = tmp >> 8;
	crc[3] = tmp & 0xFF;
	tmp = Cal_CRC16(&check_data[20],10);
	crc[4] = tmp >> 8;
	crc[5] = tmp & 0xFF;

	for(i=0;i<6;i++) {
		mac[i] = crc[i] + check_data[128+i];
	}
	if(THISINFO)printf("in the check_get_mac_addrCRC:%02X,%02X,%02X,%02X,%02X,%02X\r\n",crc[0],crc[1],crc[2],crc[3],crc[4],crc[5]);
	if(THISINFO)printf("in the check_get_mac_addrMAC:%02X,%02X,%02X,%02X,%02X,%02X\r\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	if(THISINFO)printf("in the check_get_mac_addrMAC:%d.%d.%d.%d.  %d.%d\r\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
#endif
#if 0
void check_app(void)
{
	uint16_t tmp;
	uint8_t  * pbuffer = NULL;
	addr_hig = (unsigned char)(((unsigned int)app_check_data)>>8);
	if(THISINFO)puts("in the check app");
	tmp = addr_hig;
	tmp <<= 8;
	tmp |= addr_low;
	pbuffer = (uint16_t *)tmp;
	tmp = Cal_CRC16(pbuffer,127);
	if(THISINFO)printf("buffer start address:0x%X\r\n",tmp);
	pbuffer += 127;
	{
		tmp -= Cal_CRC16(pbuffer,127);
		if(tmp != (*(uint16_t *)(pbuffer+127)) ) {
			error_flag++;
		}
	}
}
#endif

uint16_t Cal_CRC16(uint16_t * buffer,uint16_t len)
{
	uint16_t Reg_CRC = 0xffff;
    uint8_t  i, j;
    for (i = 0; i < len; i++)
    {
		uint16_t tmp = pgm_read_byte(buffer+i) + pgm_read_byte(buffer+i+1);
		Reg_CRC ^= tmp;
		for (j = 0; j < 8; j++)
		{
			if ((Reg_CRC & 0x0001) == 0x0001)
			{
				Reg_CRC = (uint16_t)(Reg_CRC >> 1 ^ (uint16_t)(0xA001));
			}
			else
			{
				Reg_CRC >>= 1;
			}
		}
	}
	return Reg_CRC;
}

#endif
