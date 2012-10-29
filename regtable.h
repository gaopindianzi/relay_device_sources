#ifndef __REG_TABLE_H__
#define __REG_TABLE_H__


typedef struct _reg_node
{
	unsigned int base_addr;
	unsigned int data_len;
	unsigned int count;
} reg_node;

extern reg_node reg_table[];

#endif   //ifdef __REG_TABLE_H__