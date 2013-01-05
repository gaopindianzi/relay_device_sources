#include "StringPrecess.h"
#include <stdio.h>
#include "debug.h"
#include <debug.h>

#define THISINFO          0
#define THISERROR         0
#define THISASSERT        0

unsigned int StringLength(const char * des)
{
    unsigned int i=0;
    while(*des++) {
        i++;
    }
    return i;
}

void StringCopy(char * des , const char * src)
{
    do {
        *des++ = *src;
    }while(*src++);
}

BOOL StringMatchHead(const char * des,const char * head)
{
	while(1) {
		if(*des != *head) {
			return FALSE;
		}
		des++;
		head++;
    if(*head == '\0') {
			return TRUE;
		}
		if(*des == '\0') {
			return FALSE;
		}
	}
}


/*
从一个字符中剪切一个字符串，以某些字符为间隔区分开来，剪切第N个间隔串
src源字符串
ch区分字符串
des目标字符串
返回获得的字符串长度
*/
unsigned char GetStringDiviseBy(const char * src,const char *ch,
						  char * des,unsigned char len,unsigned char n)
{
	unsigned char k,i;
	const char * pch;
	if(*ch == '\0') {
		return 0;
	}
	k = 0;
	while(k<n) {
		pch = ch;
		while(1) {
			if(*pch == '\0') {
				break;
			}
			if(*src == *pch) {
				k++;
				break;
			} else {
				pch++;
			}
		}
		if(*src == '\0') {
			return 0;
		}
		src++;
	}
	k = 0;
	while(1) {
		if(*src == '\0') {
			goto finish;
		}
		pch = ch;
		while(1) {
			if(*pch == '\0') {
				break;
			}
			if(*src == *pch) {
				goto finish;
			}
			pch++;
		}
		if(k<len) {
			*des = *src;
			des++;
		}
		++k;
		src++;
	}
finish:	
	i = k;
	while(i<len) {
		*des++ = '\0';
		++i;
	}
	return k;
}

unsigned char StringTrimBy(char * des,const char *ch)
{
	const char * pch;
	char * ptr;
	unsigned char i;
	i = 0;
    ptr = des;
	while(1) {
		pch = ch;
		while(1) {
			if(*pch == '\0') {
				goto step1;
			} else if(*ptr == *pch) {
				i++;
				break;
			}
			pch++;
		}
		ptr++;
	}
step1:
  pch = &des[i];
	ptr = des;
	if(i) {
		i = 0;
	  while(1) {
		  if(*pch == '\0') {
			  break;
		  }
		  *ptr++ = *pch++;
		  i++;
	  }
	} else {
		i = 0;
		while(*ptr != '\0') {
			ptr++;
			i++;
		}
	}
	//去掉尾部字符
	ptr--;
	while(1) {
		pch = ch;
		while(1) {
			if(*pch == '\0') {				
				goto step2;//没找到
			} else if(*ptr == *pch) {
				i--;//找到
				break;
			}
			pch++;
		}
		ptr--;
	}
step2:
  ptr++;
	while(*ptr != '\0') {
		*ptr++ = '\0';
	}
	return i;
}

void StringSubString(const char * src,char * des,unsigned int start,unsigned int len)
{
    unsigned int i;
    src = &src[start];
    i = 0;
    while(i<len) {
        if(*src == '\0') {
            break;
        }
        *des++ = *src++;
        i++;
    }
    *des = 0;
}


void StringReverse12(char * src)
{
    unsigned int i;
    i=0;
    while(1) {
        char tmp;
        if(src[i] == '\0') {
            break;
        }
        tmp = src[i];
        src[i] = src[i+1];
        src[i+1] = tmp;
        i += 2;
    }
}

/*
查找字符串,如果找到，返回真，如果没找到，返回假,字符串在index的位置中
*/
BOOL StringFindString(const char * src,const char * des,unsigned int * index)
{
	unsigned int src_len = StringLength(src);
	unsigned int des_len = StringLength(des);
	unsigned int i = 0;
	while(1) {
		if(src_len - i < des_len) {
            *index = 0;
			return FALSE;
		}
		if(StringMatchHead(src+i,des) == TRUE) {
			*index = i;
			return TRUE;
		}
		i++;
	}
}

BOOL IsUnicodeNumberString(const char * buffer,unsigned int len)
{
	unsigned int i = 0;
	while(1) {
		if(buffer[0] != '0' || buffer[1] != '0' || buffer[2] != '3') {
			if(buffer[3] <= '0' || buffer[3] >= '9') {
				break;
			}
		}
		i += 4;
		if(i >= len) {
			return TRUE;
		}
	}
	return FALSE;
}

void StringUnicodeNumberToAscii(const char * src,unsigned int len,char * des)
{
	unsigned int i=0;
	while(1) {
		*des++ = src[i+3];
		i += 4;
		if(i >= len) {
			break;
		}
	}
	*des = '\0';
}

void ValueIntToStringHex(char * buffer,unsigned char val)
{
    unsigned char tmp;
    tmp = (val>>4)&0xF;
    if(tmp < 10) {
        buffer[0] = '0' + tmp;
    } else {
        buffer[0] = 'A' + tmp - 10;
    }
    tmp = (val>>0)&0xF;
    if(tmp < 10) {
        buffer[1] = '0' + tmp;
    } else {
        buffer[1] = 'A' + tmp - 10;
    }
    buffer[2] = 0;
}

//一个整数值转化为整数字符串
//存储在buffer中，以'\0'为结尾
void ValueIntToStringDec(char * buffer,unsigned int val)
{
	unsigned char i = 0;
	unsigned char wei = 0;
	unsigned int  tmp = val;
	if(tmp == 0) {
		buffer[0] = '0';
		buffer[1] = '\0';
		return ;
	}
	while(1) {
		if(tmp > 0) {
		  wei++;
    } else {
			break;
		}
		tmp /= 10;
	}	
	for(i=wei;i>0;i--) {
	  unsigned char temp = val % 10;
		buffer[i-1] = temp + '0';
		val /= 10;
	}
	buffer[wei] = '\0';
	return ;
}

void ValueIntToStringBin(char * buffer,unsigned long  val)
{
	unsigned char i,index = 0;
	//ASSERT(buffer!=0);
	buffer[0] = '0';
	buffer[1] = '\0';
	i = 32;
	while(val) {
		if(val&(1ul<<31)) {
			break;
		}
		val <<= 1;
		i--;
	}
	if(!val) {
		return ;
	}
	for(;i>0;i--) {
		buffer[index++] = (val&(1ul<<31))?'1':'0';
		val <<= 1;
	}
	buffer[index++] = '\0';
}



unsigned int StringHexToValueInt(const char * str)
{
    unsigned int ret = 0;
    while(1) {
        char ch = *str;
        if(ch == '\0') {
            break;
        }
        ret <<= 4;
        if('0' <= ch && ch <= '9') {
            ret += ch - '0';
        } else if('A' <= ch && ch <= 'F') {
            ret += (ch - 'A') + 10;
        } else if('a' <= ch && ch <= 'f') {
			 ret += (ch - 'a') + 10;
		}
        str++;
    }
    return ret;
}

unsigned char StringHex2ToValueInt(const char * str)
{
	unsigned char num = 0;
    unsigned char ret = 0;
    while(1) {
        char ch = *str;
        if(ch == '\0') {
            break;
        }
        ret <<= 4;
        if('0' <= ch && ch <= '9') {
            ret += ch - '0';
        } else if('A' <= ch && ch <= 'F') {
            ret += (ch - 'A') + 10;
        } else if('a' <= ch && ch <= 'f') {
			 ret += (ch - 'a') + 10;
		}
        str++;
		if(++num >= 2) {
			break;
		}
    }
    return ret;
}


unsigned int StringBinToValueInt(const char * str)
{
	int val = 0;
	int len = StringLength(str);
	int i;
	//if(len > 16) len = 16;
	for(i=0;i<len;i++) {
		val <<= 1;
		val |= (str[i] == '1')?1:0;
	}
	return val;
}

unsigned int StringDecToValueInt(const char * str)
{
    unsigned int ret = 0;
		while(*str < '0' || *str > '9') {
			str++;
		}
    while(1) {
        char ch = *str;
        if(ch == '\0' || ch < '0' || ch > '9') {
            break;
        }
        ret *= 10;
				ret += ch - '0';
        str++;
    }
    return ret;
}

unsigned int StringAddStringLeft(char * des ,const char *src)
{
    unsigned int lendes = StringLength(des);
    unsigned int lensrc = StringLength(src);
    unsigned int len = lendes + lensrc;

    char * p1;
    const char * p2;

    if(lensrc == 0) {
        return lendes;
    }
    p1 = des+lendes;
    p2 = p1;
    p1 += lensrc;
    do {
        *p1-- = *p2--;
    }while(lendes--);
    p2 = &src[lensrc-1];
    do {
        *p1-- = *p2--;
    }while(lensrc--);
    return len;
}

unsigned int StringAddStringRight(char * des ,const char *src)
{
    unsigned int len = StringLength(des);
    des = &des[len];
    do {
        *des++ = *src;
        len++;
    }while(*src++);
    return len;
}

//--------------------------XML-----------------------------

const char * curr_pxml = NULL;

BOOL XML_Reset(const char * pxml)
{
	unsigned char ch;
	if(StringLength(pxml) == 0) {
		return FALSE;
	}
	while(1) {
		ch = *pxml;
		if(ch == '<') {
			break;
		}
		if(ch == '\0') {
			return FALSE;
		}
		pxml++;
	}
	curr_pxml = pxml;
	return TRUE;
}

BOOL XML_NextItem(char * item_name,unsigned int buf_len)
{
	unsigned int len = 0;
	char last_ch = *curr_pxml++;
	//ASSERT(last_ch == '<');
	while(1) { //</
		char ch = *curr_pxml;
		if(ch == '\0') {
			return FALSE;
		} else if(ch == ' ' || ch == '\r' || ch == '\n') {
		} else if(ch != '/' && last_ch == '<') {
			break;
		}
		curr_pxml++;
		last_ch = ch;
	}
	while(1) { //提取文字
		char ch = *curr_pxml;
		if(ch == '\0') {
			return FALSE;
		} else if(ch == '>') {
			curr_pxml++;
			break; //完毕
		} else if(ch == ' ' || ch == '\r' || ch == '\n') {
		} else {
		  if(len < buf_len - 1) {
		    *item_name++ = ch;
		  }
			len++;
		}
		curr_pxml++;
		last_ch = ch;
	}
	*item_name++ = '\0';
	return TRUE;
}
BOOL XML_GetItemValue(char * str_buf,unsigned int buf_len)
{
	char last_ch;
	unsigned int len = 0;
	//ASSERT(str_buf);
	//ASSERT(buf_len>0);
	while(buf_len) {
		char ch = *curr_pxml;
		if(ch == '\0') {
			*str_buf++ = '\0';
			return FALSE;
		} else if(ch == '\r' || ch == '\n' || ch == ' ') {
		} else if(ch == '<' && len == 0) {
			*str_buf++ = '\0';
			return FALSE;
		} else if(ch == '<' && len > 0) {
		} else if(ch == '/' && last_ch == '<') {
			break;
		} else {
		  if(len < buf_len - 1) {
		    *str_buf++ = ch;
		  }
			len++;
		}
		curr_pxml++;
		last_ch = ch;
	}
	*str_buf++ = '\0';
	return TRUE;
}


BOOL XML_GetNameValue(const char * pname,char * valbuf,unsigned int buf_len)
{
	while(1) {
		if(XML_NextItem(valbuf,buf_len) == TRUE) {
			if(StringMatchHead(valbuf, pname) == TRUE) {
				if(XML_GetItemValue(valbuf,buf_len) == TRUE) {
					if(StringLength(valbuf) > 0) {
						return TRUE;
					}
				}
			}
		} else {
		  break;
	  }
	}
	return FALSE;
}


