#ifndef __CGI_THREAD_H__
#define __CGI_THREAD_H__
#ifdef APP_CGI_ON

extern char  gpassword[32]; //��ҳ����

int ShowThreads(FILE * stream, REQUEST * req);
void StartCGIServer(void);


#endif
#endif
