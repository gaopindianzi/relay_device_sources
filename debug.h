#ifndef __DEBUG_H__
#define __DEBUG_H__

#define ASSERT(assert)   if(THISASSERT) do { if(!assert) { printf("assert failed at:%s,%s:%d\r\n",__FILE__,__FUNCTION__,__LINE__); NutSleep(10000); } }while(0)
#define DEBUGMSG(on,str) if(on) do { printf str ; }while(0)


#endif
