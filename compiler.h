#ifndef __COMPILER_H__
#define __COMPILER_H__


typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;
//typedef   BYTE   uint8_t;
//typedef   WORD   uint16_t;
//typedef   DWORD  uint32_t;
//typedef   BYTE   u8_t;
//typedef   WORD   u16_t;
//typedef   DWORD  u32_t;

extern const unsigned char  code_msk[8];

#define GET_OFFSET_MEM_OF_STRUCT(type,member)   ((unsigned char *)(&(((type *)0)->member)) - ((unsigned char *)((type *)0)))
#define GET_MEM_SIZE_OF_STRUCT(type,member)     ((unsigned char *)0 + sizeof(type) - (unsigned char *)(&(((type *)0)->member)))
#define GET_ARRRYS_NUM(type)                    (sizeof(type)/sizeof((&type)[0]))
#define SET_BIT(Bitarrys,Index,On)      do{ if(On) { (Bitarrys)[(Index)/8] |=  code_msk[(Index)%8]; } else { \
                                                    (Bitarrys)[(Index)/8] &= ~code_msk[(Index)%8]; } } while(0)
#define BIT_IS_SET(Bitarrys,Index)       (((Bitarrys)[(Index)/8]&code_msk[(Index)%8])?1:0)


//根据比特数，计算占有几个字节
#define BITS_TO_BS(bit_num)    (((bit_num)+7)/8)
//字节数组编程WORD型
#define LSB_BYTES_TO_WORD(bytes)            ((((WORD)((bytes)[1]))<<8)|(bytes)[0])
#define HSB_BYTES_TO_WORD(bytes)            ((((WORD)((bytes)[0]))<<8)|(bytes)[1])

#endif

