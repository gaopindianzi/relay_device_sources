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
#include <dev/relaycontrol.h>

#include <dev/watchdog.h>
#include <sys/timer.h>
#include "StringPrecess.h"
#include "sysdef.h"
#include "sys_var.h"
#include "io_out.h"
#include "bsp.h"

#define THISINFO           0
#define THISERROR          0


#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif

#include "debug.h"


#ifndef HTTPD_SERVICE_STACK
#if defined(__AVR__)
#define HTTPD_SERVICE_STACK ((512 * NUT_THREAD_STACK_MULT) + NUT_THREAD_STACK_ADD)
#elif defined(__arm__)
#define HTTPD_SERVICE_STACK ((1024 * NUT_THREAD_STACK_MULT) + NUT_THREAD_STACK_ADD)
#else
#define HTTPD_SERVICE_STACK  ((2048 * NUT_THREAD_STACK_MULT) + NUT_THREAD_STACK_ADD)
#endif
#endif


#include "bsp.h"


static char *html_mt = "text/html";


int ShowThreads(FILE * stream, REQUEST * req)
{
    static prog_char head[] = "<HTML><HEAD><TITLE>Threads</TITLE></HEAD><BODY><H1>Threads</H1>\r\n"
        "<TABLE BORDER><TR><TH>Handle</TH><TH>Name</TH><TH>Priority</TH><TH>Status</TH><TH>Event<BR>Queue</TH><TH>Timer</TH><TH>Stack-<BR>pointer</TH><TH>Free<BR>Stack</TH></TR>\r\n";
#if defined(__AVR__)
    static prog_char tfmt[] =
        "<TR><TD>%04X</TD><TD>%s</TD><TD>%u</TD><TD>%s</TD><TD>%04X</TD><TD>%04X</TD><TD>%04X</TD><TD>%u</TD><TD>%s</TD></TR>\r\n";
#else
    static prog_char tfmt[] =
        "<TR><TD>%08lX</TD><TD>%s</TD><TD>%u</TD><TD>%s</TD><TD>%08lX</TD><TD>%08lX</TD><TD>%08lX</TD><TD>%lu</TD><TD>%s</TD></TR>\r\n";
#endif
    static prog_char foot[] = "</TABLE></BODY></HTML>";
    static char *thread_states[] = { "TRM", "<FONT COLOR=#CC0000>RUN</FONT>", "<FONT COLOR=#339966>RDY</FONT>", "SLP" };
    NUTTHREADINFO *tdp = nutThreadList;

    /* Send HTTP response. */
    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);

    /* Send HTML header. */
    fputs_P(head, stream);
    for (tdp = nutThreadList; tdp; tdp = tdp->td_next) {
        fprintf_P(stream, tfmt, (uptr_t) tdp, tdp->td_name, tdp->td_priority,
                  thread_states[tdp->td_state], (uptr_t) tdp->td_queue, (uptr_t) tdp->td_timer,
                  (uptr_t) tdp->td_sp, (uptr_t) tdp->td_sp - (uptr_t) tdp->td_memory,
                  *((u_long *) tdp->td_memory) != DEADBEEF ? "Corr" : "OK");
    }
    fputs_P(foot, stream);
    fflush(stream);

    return 0;
}


int ShowTimers(FILE * stream, REQUEST * req)
{
    static prog_char head[] = "<HTML><HEAD><TITLE>Timers</TITLE></HEAD><BODY><H1>Timers</H1>\r\n";
    static prog_char thead[] =
        "<TABLE BORDER><TR><TH>Handle</TH><TH>Countdown</TH><TH>Tick Reload</TH><TH>Callback<BR>Address</TH><TH>Callback<BR>Argument</TH></TR>\r\n";
#if defined(__AVR__)
    static prog_char tfmt[] = "<TR><TD>%04X</TD><TD>%lu</TD><TD>%lu</TD><TD>%04X</TD><TD>%04X</TD></TR>\r\n";
#else
    static prog_char tfmt[] = "<TR><TD>%08lX</TD><TD>%lu</TD><TD>%lu</TD><TD>%08lX</TD><TD>%08lX</TD></TR>\r\n";
#endif
    static prog_char foot[] = "</TABLE></BODY></HTML>";
    NUTTIMERINFO *tnp;
    u_long ticks_left;

    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);

    /* Send HTML header. */
    fputs_P(head, stream);
    if ((tnp = nutTimerList) != 0) {
        fputs_P(thead, stream);
        ticks_left = 0;
        while (tnp) {
            ticks_left += tnp->tn_ticks_left;
            fprintf_P(stream, tfmt, (uptr_t) tnp, ticks_left, tnp->tn_ticks, (uptr_t) tnp->tn_callback, (uptr_t) tnp->tn_arg);
            tnp = tnp->tn_next;
        }
    }

    fputs_P(foot, stream);
    fflush(stream);

    return 0;
}




THREAD(Service, arg)
{
    TCPSOCKET *sock;
    FILE *stream;
    u_char id = (u_char) ((uptr_t) arg);


	DEBUGMSG(THISINFO,("Http Server start...\r\n"));

	NutThreadSetPriority(HTTP_SERVER_PRI);
    /*
     * Now loop endless for connections.
     */
    for (;;) {

        /*
         * Create a socket.
         */
        if ((sock = NutTcpCreateSocket()) == 0) {
            if(THISINFO)printf("[%u] Creating socket failed\n", id);
            NutSleep(5000);
            continue;
        }

        /*
         * Listen on port 80. This call will block until we get a connection
         * from a client.
         */
        NutTcpAccept(sock, gweb_port);
#if defined(__AVR__)
        if(THISINFO)printf("\r\n[%u] Connected, %u bytes free\r\n", id, NutHeapAvailable());
#else
        if(THISINFO)printf("\r\n[%u] Connected, %lu bytes free\n", id, NutHeapAvailable());
#endif

        /*
         * Wait until at least 8 kByte of free RAM is available. This will
         * keep the client connected in low memory situations.
         */
#if defined(__AVR__)
        while (NutHeapAvailable() < 4096) {
#else
        while (NutHeapAvailable() < 4096) {
#endif
            if(THISINFO)printf("[%u] Low mem\n", id);
            NutSleep(1000);
        }

        /*
         * Associate a stream with the socket so we can use standard I/O calls.
         */
        if ((stream = _fdopen((int) ((uptr_t) sock), "r+b")) == 0) {
            if(THISINFO)printf("[%u] Creating stream device failed\n", id);
        } else {
            /*
             * This API call saves us a lot of work. It will parse the
             * client's HTTP request, send any requested file from the
             * registered file system or handle CGI requests by calling
             * our registered CGI routine.
             */
            NutHttpProcessRequest(stream);

            /*
             * Destroy the virtual stream device.
             */
            fclose(stream);
        }

        /*
         * Close our socket.
         */
        NutTcpCloseSocket(sock);
        if(THISINFO)printf("[%u] Disconnected\n", id);
    }
}

int handle_user_quest(FILE * stream, REQUEST * req);
int web_relay_io_ctl(FILE * stream, REQUEST * req);
int change_password(FILE * stream, REQUEST * req);
//int io_control_main(FILE * stream, REQUEST * req);
#ifdef APP_HTTP_PROTOTOL_CLIENT
int main_ajax_handle(FILE * stream, REQUEST * req);
#endif




char  gpassword[32];


void StartCGIServer(void)
{
    uint8_t i;

	//
	BspReadWebPassword(gpassword);
	NutRegisterCgiBinPath("cgi-bin/");
	//
	NutRegisterCgi("io_request.cgi", web_relay_io_ctl);
	NutRegisterCgi("user_quest.cgi", handle_user_quest);
	NutRegisterCgi("change_password.cgi", change_password);
#ifdef APP_HTTP_PROTOTOL_CLIENT
	NutRegisterCgi("main_ajax_handle.cgi", main_ajax_handle);
#endif
	

    for (i = 1; i <= 3; i++) {
        char thname[] = "httpd0";

        thname[5] = '0' + i;
        NutThreadCreate(thname, Service, (void *) (uptr_t) i, 
            (HTTPD_SERVICE_STACK * NUT_THREAD_STACK_MULT) + NUT_THREAD_STACK_ADD);
    }
}

int web_relay_io_ctl(FILE * stream, REQUEST * req)
{
    /*
     * This may look a little bit weird if you are not used to C programming
     * for flash microcontrollers. The special type 'prog_char' forces the
     * string literals to be placed in flash ROM. This saves us a lot of
     * precious RAM.
     */
	
	static prog_char head[] = "<!DOCTYPE html PUBLIC ""-//W3C//DTD XHTML 1.0 Transitional//EN"" ""http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"">"
"<html xmlns=""http://www.w3.org/1999/xhtml"">"
"<head>"
"<meta http-equiv=""Content-Type"" content=""text/html; charset=gb2312"" />"
"<title>远程控制开关</title>"
"</head>"
"<body>";
    static prog_char foot[] = "</BODY></HTML>";

	if(THISINFO)printf("In CssCgiTest\r\n");

    /* These useful API calls create a HTTP response for us. */
    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);
	//CSS定义区
	//
	fputs_P(head,stream);

	//用户请求执行区
	if (req->req_query) { //如果有请求
        char *name;
        char *value;
        int i;
        int count;
        count = NutHttpGetParameterCount(req);
        /* Extract count parameters. */
		if(THISINFO)printf("Get Http arg count(%d)\r\n",count);
		//fprintf_P(stream,PSTR("status=request ok,io="));
        for (i = 0; i < count; i++) {
            name = NutHttpGetParameterName(req, i);
            value = NutHttpGetParameterValue(req, i);
            /* Send the parameters back to the client. */
			//
			if(strcmp(name,"ID") == 0) {
				int id = atoi(value);
				if(THISINFO)printf("reveice ID = %d",id);
				if(id < 1) {
					fputs_P(PSTR("status=ID parameter error! (ID must be >= 1"),stream);
					break;
				} else {
					name = NutHttpGetParameterName(req, i+1);
					value = NutHttpGetParameterValue(req, i+1);
					if(strcmp(name,"Command") == 0) {
						if(strcmp(value,"On") == 0) {
							unsigned char reg0x01 = 0x01;
							//unsigned char buf[2];
							//buf[0] = id & 0xFF;
							//buf[1] = id >> 8;

							io_out_set_bits(id-1,&reg0x01,1);

							//_ioctl(_fileno(sys_varient.iofile), IO_SET_ONEBIT, buf);
							//SetRelayOneBitWithDelay(id-1);
							fprintf_P(stream,PSTR("status=request ok,set ID=%d On"),id);
							break;
						} else if(strcmp(value,"Off") == 0) {
							unsigned char reg0x00 = 0x00;
							//ClrRelayOneBitWithDelay(id-1);
							//
							//id = id-1;
							//unsigned char buf[2];
							//buf[0] = id & 0xFF;
							//buf[1] = id >> 8;
							io_out_set_bits(id-1,&reg0x00,1);
							//_ioctl(_fileno(sys_varient.iofile), IO_CLR_ONEBIT, buf);
							//
							fprintf_P(stream,PSTR("status=request ok,set ID=%d Off"),id);
							break;
						} else {
							fputs_P(PSTR("status=failed! parameter is case sensitive!"),stream);
							break;
						}
					}
				}
				break;
			}
			//
			if(strcmp(name,"setiovalue") == 0) {
				uint8_t i;
				unsigned char buf[2];
				uint16_t out = 0;
				if(THISINFO)printf("Set IO=%s\r\n",value);
				fprintf_P(stream,PSTR("status=request ok,io="));
				fprintf_P(stream,PSTR("%s"),value);
				for(i=0;i<16;i++) {
					out >>= 1;
				    if(value[i] == '1') {
						out |= 0x8000;
					}
				}
				//SetRelayWithDelay(out);
				buf[0] = out & 0xFF;
				buf[1] = out >> 8;
				_ioctl(_fileno(sys_varient.iofile), IO_OUT_SET, buf);
				break;
			}
			if(strcmp(name,"setiomsk") == 0) {
				uint8_t i;
				uint16_t out = 0;
				unsigned char buf[2];
				if(THISINFO)printf("Set IO=%s\r\n",value);
				fprintf_P(stream,PSTR("status=request ok,io="));
				fprintf_P(stream,PSTR("%s"),value);
				for(i=0;i<16;i++) {
					out <<= 1;
				    if(value[i] == '1') {
						out |= 0x01;
					}
				}
				//_ioctl(_fileno(iofile), IO_OUT_GET, buf);
				//out |= GetIoOut();
				//SetRelayWithDelay(out);
				buf[0] |= out & 0xFF;
				buf[1] |= out >> 8;
				_ioctl(_fileno(sys_varient.iofile), IO_SET_BITMAP, buf);
				break;
			}
			if(strcmp(name,"clriomsk") == 0) {
				uint8_t i;
				uint8_t out = 0;
				unsigned char buf[2];
				if(THISINFO)printf("Set IO=%s\r\n",value);
				fprintf_P(stream,PSTR("status=request ok,io="));
				fprintf_P(stream,PSTR("%s"),value);
				for(i=0;i<16;i++) {
					out <<= 1;
				    if(value[i] == '1') {
						out |= 0x01;
					}
				}
				//i = GetIoOut();
				//i &= ~out;
				//SetRelayWithDelay(i);
				buf[0] |= out & 0xFF;
				buf[1] |= out >> 8;
				_ioctl(_fileno(sys_varient.iofile), IO_CLR_BITMAP, buf);
				break;
			}
			if(strcmp(name,"queryallout") == 0) {
				int i;
				unsigned char buf[2];
				char out[17] = {0,0,0,0,0,0,0,0};
				uint16_t io;
				_ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, buf);
				io = buf[1];
				io <<= 8;
				io |= buf[0];
				for(i=0;i<16;i++) {
					out[i] = '0';
					if(io&0x01) {
						out[i] = '1';
					}
					io >>= 1;
				}
				out[16] = 0;
				if(THISINFO)printf("Set IO=%s\r\n",out);
				fprintf_P(stream,PSTR("status=request ok,io="));
				fprintf_P(stream,PSTR("%s"),out);
				break;
			}
			if(strcmp(name,"quary_infochout") == 0) {
				//char out[64];
				uint32_t num;
				unsigned int  cnt = 0;
				_ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &num);
				cnt = (unsigned int)num;
				fprintf_P(stream,PSTR("status=request ok,io="));
				fprintf_P(stream,PSTR("%d"),cnt);
				break;
			}
		}
		fprintf_P(stream,PSTR(""));
	} else { //显示当前状态
		fputs_P(PSTR("status=no request!"),stream);
	}
    /* Send HTML footer and flush output buffer. */
    fputs_P(foot, stream);
    fflush(stream);

    return 0;
}

int handle_user_quest(FILE * stream, REQUEST * req)
{
	static prog_char head[] = "<!DOCTYPE html PUBLIC ""-//W3C//DTD XHTML 1.0 Transitional//EN"" ""http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"">"
"<html xmlns=""http://www.w3.org/1999/xhtml"">"
"<head>"
"<meta http-equiv=""Content-Type"" content=""text/html; charset=gb2312"" />"
"<title>远程控制开关</title>"
"</head>"
"<body>";
    static prog_char foot[] = "</BODY></HTML>";
	static prog_char form_change_password[] = "<form action=/cgi-bin/change_password.cgi method=GET>  "  \
"新密码: "  \
"<input type=password  name=newpassword1><br />" \
"密码重复:" \
"<input type=password  name=newpassword2><br />" \
"<input type=submit value=提交>"  \
"</form>";

	if(THISINFO)printf("In handle_user_quest\r\n");

    /* These useful API calls create a HTTP response for us. */
    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);
	//CSS定义区
	//
	fputs_P(head,stream);
	//用户请求执行区
	if (req->req_query) { //如果有请求
        char *name;
        char *value;
        int i;
        int count;
        count = NutHttpGetParameterCount(req);
        /* Extract count parameters. */
        for (i = 0; i < count; i++) {
            name = NutHttpGetParameterName(req, i);
            value = NutHttpGetParameterValue(req, i);
            /* Send the parameters back to the client. */
			if(strcmp(name,"password") == 0) {
				if(strcmp(value,gpassword) == 0) {
					fputs_P(PSTR("登陆成功<br />"),stream);
					fputs_P(PSTR("<a href=""/io_out_control.html"">进入控制界面</a><br /><br />"),stream);
#ifdef APP_HTTP_PROTOTOL_CLIENT					
					fputs_P(PSTR("<a href=""/main.html"">进入管理界面</a><br /><br />"),stream);
#endif					
					fputs_P(PSTR("修改密码<br /><br />"),stream);
					fputs_P(form_change_password,stream);
					fputs_P(PSTR("<a href=""/index.html"">返回登陆界面</a><br />"),stream);
					break;
				} else {
					goto error;
				}
			}
		}
	} else { //没有用户请求
error:
		fputs_P(PSTR("密码不对,请核对密码再次登录.<br /><br />"),stream);
		fputs_P(PSTR("<a href=""/index.html"">返回登陆界面</a><br />"),stream);
	}
    /* Send HTML footer and flush output buffer. */
    fputs_P(foot, stream);
    fflush(stream);

    return 0;
}

int change_password(FILE * stream, REQUEST * req)
{
	static prog_char head[] = "<!DOCTYPE html PUBLIC ""-//W3C//DTD XHTML 1.0 Transitional//EN"" ""http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"">"
"<html xmlns=""http://www.w3.org/1999/xhtml"">"
"<head>"
"<meta http-equiv=""Content-Type"" content=""text/html; charset=gb2312"" />"
"<title>远程控制开关</title>"
"</head>"
"<body>";
    static prog_char foot[] = "</BODY></HTML>";
    /* These useful API calls create a HTTP response for us. */
    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);
	//CSS定义区
	//
	fputs_P(head,stream);
	//用户请求执行区
	if (req->req_query) { //如果有请求
        char *name;
        char *value;
        int i;
        int count;
        count = NutHttpGetParameterCount(req);
        /* Extract count parameters. */
        for (i = 0; i < count; i++) {
            name = NutHttpGetParameterName(req, i);
            value = NutHttpGetParameterValue(req, i);
            /* Send the parameters back to the client. */
			if(strcmp(name,"newpassword1") == 0) {
				if(++i >= count) break;
                name = NutHttpGetParameterValue(req, i);
				if(strcmp(value,name) == 0) {
				    strcpy(gpassword,value);
				    if(strcmp(gpassword,value) == 0) {
						BspWriteWebPassword(gpassword);
					    fputs_P(PSTR("修改密码成功.<br /><br />"),stream);
				    } else {
					    fputs_P(PSTR("修改密码失败!<br /><br />"),stream);
				    }
				} else {
					fputs_P(PSTR("输入密码有误!<br /><br />"),stream);
				}
				//修改用户密码
				fputs_P(PSTR("<a href=""/index.html"">返回登陆界面</a><br />"),stream);
			}
		}
	} else { //没有用户请求
	}
    /* Send HTML footer and flush output buffer. */
    fputs_P(foot, stream);
    fflush(stream);

    return 0;
}

#ifdef APP_HTTP_PROTOTOL_CLIENT

ethernet_relay_info   sys_info;

int main_ajax_handle(FILE * stream, REQUEST * req)
{
	static prog_char head[] = "<!DOCTYPE html PUBLIC ""-//W3C//DTD XHTML 1.0 Transitional//EN"" ""http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"">"
"<html xmlns=""http://www.w3.org/1999/xhtml"">"
"<head>"
"<meta http-equiv=""Content-Type"" content=""text/html; charset=gb2312"" />"
"<title>remote_relay_manager</title>"
"</head>"
"<body>";
    static prog_char foot[] = "</BODY></HTML>";
    /* These useful API calls create a HTTP response for us. */
    NutHttpSendHeaderTop(stream, req, 200, "Ok");
    NutHttpSendHeaderBottom(stream, req, html_mt, -1);
	//CSS定义区
	//
	fputs_P(head,stream);
	//用户请求执行区
	if(THISINFO)printf("HTTP:main_ajax_handle.cgi\r\n");
	if (req->req_query) { //如果有请求
        char *name;
        char *value;
        int i;
        int count;
		device_info_st   devinfo;

        count = NutHttpGetParameterCount(req);
        /* Extract count parameters. */
		//fputs_P(PSTR("以下是AJAX请求参数<br/>"),stream);
		if(THISINFO)printf("HTTP:Request count=%d\r\n",count);

		BspLoadmultimgr_info(&devinfo);
		load_relay_info(&sys_info);

        for (i = 0; i < count; i++) {
            name = NutHttpGetParameterName(req, i);
            value = NutHttpGetParameterValue(req, i);
			if(THISINFO)printf("%d:%s=%s\r\n",i,name,value);
			if(strcmp(name,"name") == 0) {//设置名字
				if(strlen(value) > sizeof(devinfo.host_name)-1) value[sizeof(devinfo.host_name)-1] = '\0';
				strcpy(devinfo.host_name,value);
			}
			if(strcmp(name,"id") == 0) {//设置ID
				if(strlen(value) > sizeof(sys_info.id)-1) value[sizeof(sys_info.id)-1] = '\0';
				strcpy(sys_info.id,value);
			}
			if(strcmp(name,"en") == 0) {//设置enable
				if(strcmp(value,"t") == 0) {
					sys_info.enable = 1;
				} else {
					sys_info.enable = 0;
				}
			}
			if(strcmp(name,"addr") == 0) {//设置host_addr
				if(strlen(value) > sizeof(sys_info.host_addr)-1) value[sizeof(sys_info.host_addr)-1] = '\0';
				strcpy(sys_info.host_addr,value);
			}
			if(strcmp(name,"page") == 0) {//设置名字
				if(strlen(value) > sizeof(sys_info.web_page)-1) value[sizeof(sys_info.web_page)-1] = '\0';
				strcpy(sys_info.web_page,value);
			}
			if(strcmp(name,"port") == 0) {//设置端口号
				sys_info.port = StringDecToValueInt(value);
			}
			if(strcmp(name,"upt") == 0) {//设置名字
				sys_info.up_time_interval = StringDecToValueInt(value);
			}
		}
		save_relay_info(&sys_info);
		BspSavemultimgr_info(&devinfo);
		//输出返回信息
		fputs_P(PSTR("a=a"),stream);
		fprintf_P(stream,PSTR("&name=%s"),devinfo.host_name);
		fputs_P(PSTR("&b=b"),stream);
		fprintf_P(stream,PSTR("&id=%s"),sys_info.id);
		fputs_P(PSTR("&c=c"),stream);
		fprintf_P(stream,PSTR("&en=%s"),sys_info.enable?"t":"f");
		fprintf_P(stream,PSTR("&addr=%s"),sys_info.host_addr);
		fprintf_P(stream,PSTR("&page=%s"),sys_info.web_page);
		{
			char buffer[8];
		    ValueIntToStringDec(buffer,sys_info.port);
			fprintf_P(stream,PSTR("&port=%s"),buffer);
		    ValueIntToStringDec(buffer,sys_info.up_time_interval);
			fprintf_P(stream,PSTR("&upt=%s"),buffer);
		}
		//fprintf_P(stream,PSTR("&pwd=%s"),sys_info.remote_password);
		fputs_P(PSTR("&e=e"),stream);
	} else { //没有用户请求
		if(THISINFO)printf("No Request.\r\n");
		fputs_P(PSTR("b=b"),stream);
		fputs_P(PSTR("&e=e"),stream);
	}
    /* Send HTML footer and flush output buffer. */
    fputs_P(foot, stream);
    fflush(stream);
    return 0;
}

#endif
