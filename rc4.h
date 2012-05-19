#ifndef __RC4_H__
#define __RC4_H__

void init_kbox(unsigned char * key,unsigned int keylen);
void init_sbox(void);
void rc4_encrypt(unsigned char *source, unsigned char *target, int len);


#endif

