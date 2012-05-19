
#include <compiler.h>
#include <cfg/os.h>

#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>

#include <dev/board.h>
#include <dev/urom.h>
#include <dev/nplmmc.h>
#include <dev/sbimmc.h>
#include <fs/phatfs.h>

#include <sys/version.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/heap.h>
#include <sys/confnet.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/route.h>
#include <netdb.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>

#include <dev/watchdog.h>
#include <sys/timer.h>
#include <dev/ds1307rtc.h>
#include <dev/rtc.h>
#include <time.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "bsp.h"
#include "sysdef.h"

#include "des.h"


#include "debug.h"

#define THISINFO        0
#define THISASSERT      1



#define      SBOX_LEN      256
unsigned char sbox[SBOX_LEN];
unsigned char kbox[SBOX_LEN];

void init_kbox(unsigned char * key,unsigned int keylen)
{
    int i, j = 0;
    
    /*
     * ������Կ���ĳ��Ƚ϶̣������ڳ�ʼ��ʱ��ѭ�����õġ�
     */
    for (i = 0; i < SBOX_LEN; i++)
        kbox[i] = key[i % keylen];
}


/*
 * �ڳ�ʼ���Ĺ����У���Կ����Ҫ�����ǽ�S-box���ң�iȷ��S-box��
 * ÿ��Ԫ�ض��õ�����j��֤S-box�Ľ���������ġ�����ͬ��S-box
 * �ھ���α��������������㷨�Ĵ������Եõ���ͬ������Կ���У�
 * ���ң�������������ģ�
 */
void init_sbox(void)
{
    int i, j = 0;
    unsigned char tmp;
    
    for (i = 0; i < SBOX_LEN; i++)
        sbox[i] = i;

    for (i = 0; i < SBOX_LEN; i++)
    {
        j = (j + sbox[i] + kbox[i]) % SBOX_LEN;
        tmp = sbox[i];
        sbox[i] = sbox[j];
        sbox[j] = tmp;
    }
}

void rc4_encrypt(unsigned char *source, unsigned char *target, int len)
{
    int i = 0, j = 0, t, index = 0;
    unsigned char tmp;
    
    if (source == NULL || target == NULL || len <= 0)
        return;
    
    while (index < len)
    {
        i = (i + 1) % SBOX_LEN;
        j = (j + sbox[i]) % SBOX_LEN;
        tmp = sbox[i];
        sbox[i] = sbox[j];
        sbox[j] = tmp;
        t = (sbox[i] + sbox[j]) % SBOX_LEN;
        
        target[index] = source[index] ^ sbox[t]; //�������
        index++;
    }
}

