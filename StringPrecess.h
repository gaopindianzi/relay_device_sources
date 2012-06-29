#ifndef __STRINGPRECESS_H__
#define __STRINGPRECESS_H__

#ifndef BOOL
#define BOOL          unsigned char
#define TRUE          1
#define FALSE         0
#endif

unsigned int StringLength(const char * des);
void StringCopy(char * des , const char * src);
BOOL StringMatchHead(const char * des,const char * head);
unsigned char StringTrimBy(char * des,const char *ch);
unsigned char GetStringDiviseBy(const char * src,const char *ch,
						  char * des,unsigned char len,unsigned char n);
//StringSubString, start从0开始，des可能填满，结尾0可能越界,用户必须预留
void StringSubString(const char * src,char * des,unsigned int start,unsigned int len); 
void StringReverse12(char * src);
void ValueIntToStringHex(char * buffer,unsigned char val);
void ValueIntToStringDec(char * buffer,unsigned int  val);
void ValueIntToStringBin(char * buffer,unsigned long  val);
unsigned int StringHexToValueInt(const char * str);
unsigned char StringHex2ToValueInt(const char * str);
unsigned int StringBinToValueInt(const char * str);
unsigned int StringAddStringLeft(char * des ,const char *src);
unsigned int StringAddStringRight(char * des ,const char *src);
unsigned int StringDecToValueInt(const char * str);
BOOL StringFindString(const char * src,const char * des,unsigned int * index);
BOOL IsUnicodeNumberString(const char * buffer,unsigned int len);
//用户必须保证des能容纳unicode转换过去的长度，4字节转为1字节
void StringUnicodeNumberToAscii(const char * src,unsigned int len,char * des);
//XML
BOOL XML_Reset(const char * pxml);
BOOL XML_GetNameValue(const char * pname,char * valbuf,unsigned int buf_len);
							
#endif