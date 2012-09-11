
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
#include <errno.h>

#include <pro/httpd.h>
#include <pro/dhcp.h>
#include <pro/ssi.h>
#include <pro/asp.h>
#include <pro/discover.h>
#include <dev/reset_avr.h>
#include <dev/board.h>
#include <dev/gpio.h>
#include <cfg/arch/avr.h>
#include <dev/nvmem.h>

#ifdef NUTDEBUG
#include <sys/osdebug.h>
#include <net/netdebug.h>
#endif


#include <dev/relaycontrol.h>

#include "sysdef.h"
#include "bin_command_def.h"
#include "time_handle.h"
#include "io_out.h"
#include "io_time_ctl.h"
#include "sys_var.h"
#include "bsp.h"

#define THISINFO          1
#define THISERROR         1


int BspReadBoardInfo(CmdBoardInfo * info);
int BspWriteBoardInfo(CmdBoardInfo * info);
int BspReadIoName(uint8_t addr[2],CmdIoName * io);
int BspWriteIoName(uint8_t addr[2],CmdIoName * io);

int BspReadIpConfig(CmdIpConfigData * cid);
int BspWriteIpConfig(CmdIpConfigData * cid);


void RevertRelayWithDelay(uint32_t out);


typedef struct _CmdMapToCmdCallType
{
  uint8_t  cmd_id;
  int      (*CmdCall)(TCPSOCKET * sock,CmdHead * cmd,int datasize);
} CmdMapToCmdCallType;

void BinCmdPrase(TCPSOCKET * sock,void * buff,int len);

uint16_t gwork_port = 2000;
uint16_t gweb_port  = 80;

void bin_cmd_thread_server(void)
{
	uint8_t count = 0;
	TCPSOCKET * sock;
	char buff[120];
	uint32_t time = 16000;

	if(THISINFO)printf("CMD:Thraed running...\r\n");

	while(1) {
		//if(THISINFO)printf("Start Create Socket(%d)\r\n",count++);
		sock = NutTcpCreateSocket();
		if(sock == 0) {
			if(THISERROR)printf("CMD:Create socket failed!\r\n");
			continue;
		}
		NutTcpSetSockOpt(sock,SO_RCVTIMEO,&time,sizeof(uint32_t));
		if(THISINFO)printf("CMD: NutTcpAccept at port %d\r\n",gwork_port);
		if(NutTcpAccept(sock,gwork_port)) {
			NutTcpCloseSocket(sock);
			if(THISERROR)printf("CMD:NutTcpAccept Timeout! NutTcpCloseSocket and reaccept.\r\n");
			continue;
		}
		if(THISINFO)printf("CMD:Tcp Accept one connection.\r\n");
		count = 0;
		while(1) {
			int len = NutTcpReceive(sock,buff,sizeof(buff));
            if(len == 0) {
				if(THISINFO)printf("Tcp Recieve timeout.\r\n");
				if(++count == 1) {
					if(THISINFO)printf("Tcp Send ""OK"".\r\n");
				    NutTcpSend(sock,"OK",strlen("OK"));
				}
				if(++count >= 2) {
					if(THISINFO)printf("Close Command Socket\r\n");
					break;
				}
			} else if(len == -1) {
				int error = NutTcpError(sock);
				if(THISERROR)printf("CMD:Tcp Receive ERROR(%d)\r\n",error);
				if(error == ENOTCONN)  {
					if(THISERROR)printf("CMD:Socket is not connected,break connecting\r\n");
					break;
				} else {
					if(THISERROR)printf("CMD:Socket is unknow error,break connecting\r\n");
					break;
				}
			} else if(len > 0) {
				//printf("Get One Tcp packet length(%d)\r\n",len);
				BinCmdPrase(sock,buff,len);
			}
		}
		NutTcpCloseSocket(sock);
	}
thread_stop:
	while(1)NutSleep(100000);
}

static unsigned char CalCheckSum(void * buffer,unsigned int len)
{
  unsigned char sum = 0;
  unsigned int  i;
  for(i=0;i<len;i++) {
    sum += ((unsigned char *)buffer)[i];
  }
  return sum;
}

//--------------------------------------------------------------------
int CmdGetIoOutValue(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int rc = -1;
	const uint8_t outsize = sizeof(CmdIoValue);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead       * tcmd  = (CmdHead *)buffer;
	CmdIoValue    *  sio  = (CmdIoValue *)GET_CMD_DATA(tcmd);
	//
	uint32_t tmp;
    datasize = datasize;
    //
    //sio->io_count    = 32;
    //sio->io_value[0] = (uint8_t)((relay_msk >> 0) & 0xFF);
    //sio->io_value[1] = (uint8_t)((relay_msk >> 8) & 0xFF);;
    //sio->io_value[2] = (uint8_t)((relay_msk >> 16) & 0xFF);;
    //sio->io_value[3] = (uint8_t)((relay_msk >> 24) & 0xFF);;
	rc = _ioctl(_fileno(sys_varient.iofile), GET_OUT_NUM, &tmp);
	sio->io_count = (unsigned char)tmp;
	rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, sio->io_value);
    //
    tcmd->cmd_option    = CMD_ACK_OK;
    tcmd->cmd           = CMD_GET_IO_OUT_VALUE;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(sio,outsize);
    
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdGetIoOutValue OK\r\n");
    return rc;
}

int CmdSetIoOutValue(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int rc = -1;
	const uint8_t outsize = sizeof(CmdIoValue);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead       * tcmd  = (CmdHead *)buffer;
    CmdIoValue    *  sio  = (CmdIoValue *)GET_CMD_DATA(tcmd);
    //
    CmdIoValue    *   io  = (CmdIoValue *)GET_CMD_DATA(cmd);
    //
    if(datasize < sizeof(CmdIoValue)) {
        if(THISERROR)printf("ERROR:Cmd Set Io Value Datasize ERROR\r\n");
        return -1;
    }
    if(io->io_count <= 32) {
		//uint32_t tmp;
		//SetRelayWithDelay(group_arry4_to_uint32(io->io_value));

		rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_SET, io->io_value);
		rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, sio->io_value);

        tcmd->cmd_option     = CMD_ACK_OK;
        sio->io_count        = io->io_count;
		//tmp = GetIoOut();
        //sio->io_value[0] = (uint8_t)((tmp >> 0) & 0xFF);
	    //sio->io_value[1] = (uint8_t)((tmp >> 8) & 0xFF);
	    //sio->io_value[2] = (uint8_t)((tmp >> 16) & 0xFF);
	    //sio->io_value[3] = (uint8_t)((tmp >> 24) & 0xFF);
    } else {
        if(THISERROR)printf("Cmd Set Io Io Count Error!!\r\n");
        tcmd->cmd_option  = CMD_ACK_KO;
        sio->io_count = 0;
    }
    tcmd->cmd           = CMD_SET_IO_OUT_VALUE;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(sio,outsize);
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdSetIoOutValue()\r\n");
    return rc;
}

int CmdRevertIoOutIndex(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int rc = -1;
	const uint8_t outsize = sizeof(CmdIobitmap);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead          * tcmd  = (CmdHead *)buffer;
    CmdIobitmap      *  sio  = (CmdIobitmap *)GET_CMD_DATA(tcmd);
    CmdIobitmap      *   io  = (CmdIobitmap *)GET_CMD_DATA(cmd);
    //
    if(datasize < sizeof(CmdIobitmap)) {
      if(THISERROR)printf("ERROR:Cmd CmdSetIoOutBit Datasize ERROR\r\n");
	  tcmd->cmd_option    = CMD_ACK_KO;
      goto error;
    }
    //
    tcmd->cmd_option    = CMD_ACK_OK;	
	rc = _ioctl(_fileno(sys_varient.iofile), IO_SIG_BITMAP, io->io_msk);
	rc = _ioctl(_fileno(sys_varient.iofile), IO_OUT_GET, sio->io_msk);
error:
    tcmd->cmd           = CMD_REV_IO_SOME_BIT;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(sio,outsize);
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdSetIoOutBit()!\r\n");
    return rc;
}

int CmdGetIoInValue(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int rc = -1;
	uint32_t tmp;
	const uint8_t outsize = sizeof(CmdIoValue);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead       * tcmd  = (CmdHead *)buffer;
    CmdIoValue    *  sio  = (CmdIoValue *)GET_CMD_DATA(tcmd);
    //
    datasize = datasize;
    //
	rc = _ioctl(_fileno(sys_varient.iofile), GET_IN_NUM, &tmp);
	sio->io_count = (unsigned char)tmp;
	memset(sio->io_value,0,sizeof(sio->io_value));
	if(tmp) {
		rc = _ioctl(_fileno(sys_varient.iofile), IO_IN_GET, sio->io_value);
	}

    //sio->io_count    = 8;
    //sio->io_value[0] = (uint32_t)((GetFilterInput() >> 0) & 0xFF);
    //sio->io_value[1] = (uint32_t)((GetFilterInput() >> 8) & 0xFF);
    //sio->io_value[2] = (uint32_t)((GetFilterInput() >> 16) & 0xFF);
    //sio->io_value[3] = (uint32_t)((GetFilterInput() >> 24) & 0xFF);
    //
    tcmd->cmd_option    = CMD_ACK_OK;
    tcmd->cmd           = CMD_GET_IO_IN_VALUE;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(sio,outsize);
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));

    if(THISINFO)printf("CmdGetIoInValue()!\r\n");
    return rc;
}


int CmdGetIpConfig(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = sizeof(CmdIpConfigData);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdIpConfigData    *  sio  = (CmdIpConfigData *)GET_CMD_DATA(tcmd);
    //
    datasize = datasize;
    //
    if(!BspReadIpConfig(sio)) {
      tcmd->cmd_option    = CMD_ACK_OK;
    } else {
      tcmd->cmd_option    = CMD_ACK_KO;
    }
    //
    tcmd->cmd           = CMD_GET_IP_CONFIG;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = sizeof(CmdIpConfigData);
    tcmd->data_checksum = CalCheckSum(sio,sizeof(CmdIpConfigData));
	//
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdGetIpConfig()\r\n");
    return 0;
}

int CmdSetIpConfig(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = 0;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdIpConfigData    *   io  = (CmdIpConfigData *)GET_CMD_DATA(cmd);
    //
    if(datasize < sizeof(CmdIpConfigData)) {
      if(THISERROR)printf("ERROR:Cmd CmdSetIpConfigData Datasize ERROR,size(%d)",datasize);
      goto error;
    }
    //
    if(!BspWriteIpConfig(io)) {
      tcmd->cmd_option    = CMD_ACK_OK;
    } else {
error:
      tcmd->cmd_option    = CMD_ACK_KO;
    }
    //
    tcmd->cmd           = CMD_SET_IP_CONFIG;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = 0;
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdSetIpConfig()\r\n");
    return 0;
}


int CmdGetInputCtlModeIndex(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = sizeof(CmdInputModeIndex);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdInputModeIndex  *  sio  = (CmdInputModeIndex *)GET_CMD_DATA(tcmd);
    CmdInputModeIndex  *   io  = (CmdInputModeIndex *)GET_CMD_DATA(cmd);
    //
    datasize = datasize;
    //
    sio->index = io->index;
    if(!BspReadManualCtlModeIndex(io->index,&sio->mode)) {
      tcmd->cmd_option    = CMD_ACK_OK;
    } else {
      if(THISINFO)printf("BspReadManualCtlModeIndex Failed!!!\r\n");
      tcmd->cmd_option    = CMD_ACK_KO;
    }
    tcmd->cmd           = CMD_GET_INPUT_CTL_MODE_INDEX;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = sizeof(CmdInputModeIndex);
    tcmd->data_checksum = CalCheckSum(sio,sizeof(CmdInputModeIndex));
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdGetInputCtlModeIndex()\r\n");
    return 0;
}

int CmdSetInputCtlModeIndex(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = 0;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdInputModeIndex  *   io  = (CmdInputModeIndex *)GET_CMD_DATA(cmd);
    //
    datasize = datasize;
    if(cmd->cmd_len < sizeof(CmdInputModeIndex)) {
      if(THISERROR)printf("ERROR:CmdSetInputCtlModeIndex cmd->cmd_len ERROR,size(%d)\r\n",cmd->cmd_len);
	  goto error;
    }
    //
    if(!BspWriteManualCtlModeIndex(io->index,io->mode)) {
      tcmd->cmd_option    = CMD_ACK_OK;
    } else {
	  if(THISINFO)printf("BspWriteManualCtlMode Failed!!!\r\n");
error:
      tcmd->cmd_option    = CMD_ACK_KO;
    }
    tcmd->cmd           = CMD_SET_INPUT_CTL_MODE_INDEX;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = 0;
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdSetInputCtlModeIndex()\r\n");
    return 0;
}

int CmdGetIoName(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = sizeof(CmdIoName);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdIoName          *  tio  = (CmdIoName *)GET_CMD_DATA(tcmd);
	CmdIoName          *  rio  = (CmdIoName *)GET_CMD_DATA(cmd);
    //
    datasize = datasize;
    //

    if(!BspReadIoName(rio->io_addr,tio)) {
		//sprintf(tio->io_name,"INPUT%d",rio->io_addr[0]);
        tcmd->cmd_option    = CMD_ACK_OK;
    } else {
      if(THISINFO)printf("CmdGetIoName Failed!!!\r\n");
      tcmd->cmd_option    = CMD_ACK_KO;
    }
	tio->io_addr[0] = rio->io_addr[0];
	tio->io_addr[1] = rio->io_addr[1];
	//
    tcmd->cmd           = CMD_GET_IO_NAME;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(tio,outsize);
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdGetIoName()\r\n");
    return 0;
}


int CmdSetIoName(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = sizeof(CmdIoName);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead            * tcmd  = (CmdHead *)buffer;
    CmdIoName          *  tio  = (CmdIoName *)GET_CMD_DATA(tcmd);
	CmdIoName          *  rio  = (CmdIoName *)GET_CMD_DATA(cmd);
    //
    datasize = datasize;
    //
	if(THISINFO)printf("WriteIoname:addr[0]=%d,addr[1]=%d,name=%s",rio->io_addr[0],rio->io_addr[1],rio->io_name);
    if(!BspWriteIoName(rio->io_addr,rio)) {
		memcpy(tio,rio,sizeof(CmdIoName));
        tcmd->cmd_option    = CMD_ACK_OK;
    } else {
      if(THISERROR)printf("CmdGetIoName Failed!!!\r\n");
      tcmd->cmd_option    = CMD_ACK_KO;
    }
	//
    tcmd->cmd           = CMD_SET_IO_NAME;
    tcmd->cmd_index     = cmd->cmd_index;
    tcmd->cmd_len       = outsize;
    //
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    if(THISINFO)printf("CmdGetIoName()\r\n");
    return 0;
}
//--------------------------------------------------------------------------------------------
// 定时器读写命令

int CmdIoFinish(TCPSOCKET * sock,CmdHead * tcmd,int outsize)
{
    tcmd->cmd_len       = outsize;
    tcmd->data_checksum = CalCheckSum(GET_CMD_DATA(tcmd),outsize);
    NutTcpSend(sock,tcmd,(int)(sizeof(CmdHead)+outsize));
    //if(THISINFO)printf("CmdIoFinish()\r\n");
    return 0;
}

#ifdef APP_TIMEIMG_ON
int CmdGetIoTimingInfo(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	static int   time_count,time_max;
	const uint8_t outsize = (sizeof(timint_info)>=sizeof(timing_node))?sizeof(timint_info):sizeof(timing_node);
	uint8_t  act_size = outsize;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead  * tcmd  = (CmdHead *)buffer;
	//
    datasize = datasize;
    //
	SET_CMD_OK(tcmd,0);
	if(GET_CMD_STATE(cmd) == CMD_REQ_START) {
		if(THISINFO)printf("Start Read.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_START);
		//读数据
		act_size = sizeof(timint_info);
		timint_info          *  tio  = (timint_info *)GET_CMD_DATA(tcmd);
		if(!BspReadIoTimingInfo(tio)) {
			time_max = tio->time_max_count;
			time_count = 0;
			SET_CMD_OK(tcmd,1);
			if(THISINFO)printf("get timint_info ok!\r\n");
		} else {
			if(THISINFO)printf("get timint_info error!\r\n");
		}
	} else if(GET_CMD_STATE(cmd) == CMD_REQ_NEXT) {
		timing_node_eeprom  time;
		if(THISINFO)printf("Next Read.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DOING);
		//读数据
		act_size = sizeof(timing_node);
		timing_node          *  tio  = (timing_node *)GET_CMD_DATA(tcmd);
		if(time_count < time_max) {
		    if(!BspReadIoTiming(time_count,&time)) {
				memcpy(tio,&time.node,sizeof(timing_node));
			    SET_CMD_OK(tcmd,1);
			    if(THISINFO)printf("get timing_node ok!\r\n");
			} else {
				if(THISINFO)printf("get timing_node error!\r\n");
			}
		}
		time_count++;
	} else if(GET_CMD_STATE(cmd) == CMD_REQ_DONE) {
		if(THISINFO)printf("Done Read.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DONE);
		//读数据
		act_size = 0;
		SET_CMD_OK(tcmd,1);
	}
    tcmd->cmd           = CMD_GET_TIMING_INFO;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}

void dump_timing_node(timing_node * pt);

int CmdSetIoTimingInfo(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = 0;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead  * tcmd  = (CmdHead *)buffer;
	uint8_t  state = GET_CMD_STATE(cmd);
	SET_CMD_OK(tcmd,0);
	if(state == CMD_REQ_START) {
		if(THISINFO)printf("Start Write.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_START);
		if(datasize >= sizeof(timint_info)) {
		    timint_info *  rio  = (timint_info *)GET_CMD_DATA(cmd);
		    //执行
		    SET_CMD_OK(tcmd,1);
		    BspTimingStartWrite();
		    if(THISINFO)printf("Write timing info(max:%d)\r\n",rio->time_max_count);
		    if(!BspWriteIoTimingInfo(rio)) {
			    SET_CMD_OK(tcmd,1);
		    } else {
			    if(THISERROR)printf("ERROR:BspWriteIoTimingInfo failed!\r\n");
		    }
		} else {
			if(THISERROR)printf("ERROR:BspWriteIoTimingInfo datasize < timint_info failed!\r\n");
		}
	} else if(state == CMD_REQ_NEXT) {
		if(THISINFO)printf("Next Write.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DOING);
		if(datasize >= sizeof(CmdTimingNode)) {
		    timing_node_eeprom  wn;
		    CmdTimingNode * rn = (CmdTimingNode *)GET_CMD_DATA(cmd);
		    dump_timing_node(&rn->node);
		    memcpy(&(wn.node),&(rn->node),sizeof(timing_node));
		    if(!BspWriteIoTiming(rn->index,&wn)) {
			    SET_CMD_OK(tcmd,1);
		    } else {
			    if(THISERROR)printf("ERROR:BspWriteIoTiming failed!\r\n");
		    }
		} else {
			if(THISERROR)printf("ERROR:BspWriteIoTimingInfo datasize < CmdTimingNode failed!\r\n");
		}
	} else if(state == CMD_REQ_DONE) {
		if(THISINFO)printf("Done Write.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DONE);
		if(!BspTimingDoneWrite()) {
			SET_CMD_OK(tcmd,1);
			timing_load_form_eeprom();
		} else {
			if(THISERROR)printf("ERROR:BspTimingDoneWrite failed!\r\n");
		}
	}
    tcmd->cmd           = CMD_SET_TIMING_INFO;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,outsize);
}
#endif

//--------------------------------------------------------------------------------------------
//
//
int CmdSetNewRtcValue(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = 0;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead  * tcmd  = (CmdHead *)buffer;
	uint8_t  state = GET_CMD_STATE(cmd);
	SET_CMD_OK(tcmd,0);
	if(state == CMD_REQ_START) {
		if(THISINFO)printf("Start Write RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_START);
		if(datasize >= sizeof(timint_info)) {
		    time_type *  rio  = (time_type *)GET_CMD_DATA(cmd);
		    //执行
			if(!update_new_rtc_value(rio)) {
		        SET_CMD_OK(tcmd,1);
			}
		} else {
			if(THISERROR)printf("ERROR:BspWriteIoTimingInfo datasize < timint_info failed!\r\n");
		}
	} else if(state == CMD_REQ_NEXT) {
		if(THISINFO)printf("Next Write RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DOING);
		SET_CMD_OK(tcmd,1);
	} else if(state == CMD_REQ_DONE) {
		if(THISINFO)printf("Done Write RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DONE);
		SET_CMD_OK(tcmd,1);
	}
    tcmd->cmd           = CMD_SET_RTC_VALUE;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,outsize);
}
//--------------------------------------------------------------------------------------------
//
//
int CmdGetRtcValue(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	const uint8_t outsize = sizeof(time_type);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
    CmdHead  * tcmd  = (CmdHead *)buffer;
	uint8_t  state = GET_CMD_STATE(cmd);
	SET_CMD_OK(tcmd,0);
	if(state == CMD_REQ_START) {
		if(THISINFO)printf("Start Read RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_START);
		time_type *  rio  = (time_type *)GET_CMD_DATA(tcmd);
		//
		if(!raed_system_time_value(rio)) {
			SET_CMD_OK(tcmd,1);
		} else {
			if(THISERROR)printf("Read RTC Value failed!\r\n");
		}
		//
	} else if(state == CMD_REQ_NEXT) {
		if(THISINFO)printf("Next Raed RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DOING);
		SET_CMD_OK(tcmd,1);
	} else if(state == CMD_REQ_DONE) {
		if(THISINFO)printf("Done Read RTC.\r\n");
		SET_CMD_STATE(tcmd,CMD_CURRENT_DONE);
		SET_CMD_OK(tcmd,1);
	}
    tcmd->cmd           = CMD_GET_RTC_VALUE;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,outsize);
}
//--------------------------------------------------------------------------------------------
//
//

int CmdSetInputValidMsk(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int act_size = 0;
	const uint8_t outsize = sizeof(CmdIoValidMsk);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	CmdIoValidMsk    *  rio  = (CmdIoValidMsk *)GET_CMD_DATA(cmd);
	CmdIoValidMsk    *  tio  = (CmdIoValidMsk *)GET_CMD_DATA(tcmd);

	act_size = outsize;
	//
	tcmd->cmd_option    = CMD_ACK_KO;
    if(cmd->cmd_len < sizeof(CmdIoValidMsk)) {
      if(THISERROR)printf("ERROR:CmdSetInputValidMsk cmd->cmd_len ERROR,size(%d)\r\n",cmd->cmd_len);
	  goto error;
    }
	uint32_t onmsk,offmsk;
	SetInputOnMsk(group_arry4_to_uint32(rio->valid_msk),group_arry4_to_uint32(rio->invalid_msk));
	GetInputOnMsk(&onmsk,&offmsk);
	tio->valid_msk[0] = (uint8_t)((onmsk >> 0)&0xFF);
	tio->valid_msk[1] = (uint8_t)((onmsk >> 8)&0xFF);
	tio->valid_msk[2] = (uint8_t)((onmsk >> 16)&0xFF);;
	tio->valid_msk[3] = (uint8_t)((onmsk >> 24)&0xFF);
	tio->invalid_msk[0] = (uint8_t)((offmsk >> 0)&0xFF);
	tio->invalid_msk[1] = (uint8_t)((offmsk >> 8)&0xFF);
	tio->invalid_msk[2] = (uint8_t)((offmsk >> 16)&0xFF);
	tio->invalid_msk[3] = (uint8_t)((offmsk >> 24)&0xFF);
    //
    tcmd->cmd_option    = CMD_ACK_OK;
error:
    tcmd->cmd           = CMD_SET_INPUT_CTL_ON_MSK;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}


int CmdGetInputValidMsk(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int act_size = 0;
	const uint8_t outsize = sizeof(CmdIoValidMsk);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	CmdIoValidMsk    *  tio  = (CmdIoValidMsk *)GET_CMD_DATA(tcmd);
	act_size = outsize;

	uint32_t onmsk,offmsk;
	GetInputOnMsk(&onmsk,&offmsk);
	tio->valid_msk[0] = (uint8_t)((onmsk >> 0)&0xFF);
	tio->valid_msk[1] = (uint8_t)((onmsk >> 8)&0xFF);
	tio->valid_msk[2] = (uint8_t)((onmsk >> 16)&0xFF);;
	tio->valid_msk[3] = (uint8_t)((onmsk >> 24)&0xFF);
	tio->invalid_msk[0] = (uint8_t)((offmsk >> 0)&0xFF);
	tio->invalid_msk[1] = (uint8_t)((offmsk >> 8)&0xFF);
	tio->invalid_msk[2] = (uint8_t)((offmsk >> 16)&0xFF);
	tio->invalid_msk[3] = (uint8_t)((offmsk >> 24)&0xFF);
    //
    tcmd->cmd_option    = CMD_ACK_OK;

    tcmd->cmd           = CMD_GET_INPUT_CTL_ON_MSK;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}


#ifdef APP_TIMEIMG_ON
//--------------------------------------------------------------------------------------------
//
//
int CmdSetTimingValidMsk(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	uint32_t tmp1,tmp2;
	int act_size = 0;
	const uint8_t outsize = sizeof(CmdIoValidMsk);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	CmdIoValidMsk    *  rio  = (CmdIoValidMsk *)GET_CMD_DATA(cmd);
	CmdIoValidMsk    *  tio  = (CmdIoValidMsk *)GET_CMD_DATA(tcmd);

	act_size = outsize;
	//
	tcmd->cmd_option    = CMD_ACK_KO;
    if(cmd->cmd_len < sizeof(CmdIoValidMsk)) {
      if(THISERROR)printf("ERROR:CmdSetInputValidMsk cmd->cmd_len ERROR,size(%d)\r\n",cmd->cmd_len);
	  goto error;
    }
	SetTimingOnMsk(group_arry4_to_uint32(rio->valid_msk),group_arry4_to_uint32(rio->invalid_msk));
	GetTimingOnMsk(&tmp1,&tmp2);

	tio->valid_msk[0] = tmp1;
	tio->valid_msk[1] = tmp1 >> 8;
	tio->valid_msk[2] = tmp1 >> 16;
	tio->valid_msk[3] = tmp1 >> 16;
	tio->invalid_msk[0] = tmp2;
	tio->invalid_msk[1] = tmp2 >> 8;
	tio->invalid_msk[2] = tmp2 >> 16;
	tio->invalid_msk[3] = tmp2 >> 24;
    //
    tcmd->cmd_option    = CMD_ACK_OK;
error:
    tcmd->cmd           = CMD_SET_TIMING_ON_MSK;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}


int CmdGetTimingValidMsk(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	uint32_t tmp1,tmp2;
	int act_size = 0;
	const uint8_t outsize = sizeof(CmdIoValidMsk);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	CmdIoValidMsk    *  tio  = (CmdIoValidMsk *)GET_CMD_DATA(tcmd);
	act_size = outsize;

	GetTimingOnMsk(&tmp1,&tmp2);

	tio->valid_msk[0] = tmp1;
	tio->valid_msk[1] = tmp1 >> 8;
	tio->valid_msk[2] = tmp1 >> 16;
	tio->valid_msk[3] = tmp1 >> 16;
	tio->invalid_msk[0] = tmp2;
	tio->invalid_msk[1] = tmp2 >> 8;
	tio->invalid_msk[2] = tmp2 >> 16;
	tio->invalid_msk[3] = tmp2 >> 24;
    //
    tcmd->cmd_option    = CMD_ACK_OK;

    tcmd->cmd           = CMD_GET_TIMING_ON_MSK;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}
#endif

//--------------------------------------------------------------------------------------------
//
//
int CmdSetRemoteHostAddress(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int act_size = 0;
	const uint8_t outsize = 0;  //sizeof(host_address);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	host_address     *  rio  = (host_address *)GET_CMD_DATA(cmd);
	//host_address     *  tio  = (host_address *)GET_CMD_DATA(tcmd);

	act_size = 0;
	//
	tcmd->cmd_option    = CMD_ACK_KO;
    if(cmd->cmd_len < sizeof(host_address)) {
      if(THISERROR)printf("ERROR:CmdSetRemoteHostAddress cmd->cmd_len ERROR,size(%d)\r\n",cmd->cmd_len);
	  goto error;
    }
    //
	tcmd->cmd_option    = CMD_ACK_KO;
	if(!BspWriteHostAddress(rio)) {
        tcmd->cmd_option    = CMD_ACK_OK;
	}
error:
    tcmd->cmd           = CMD_SET_HOST_ADDRESS;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}
int CmdGetRemoteHostAddress(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int act_size = 0;
	const uint8_t outsize = sizeof(host_address);
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	//host_address     *  rio  = (host_address *)GET_CMD_DATA(cmd);
	host_address     *  tio  = (host_address *)GET_CMD_DATA(tcmd);
	//
	//if(THISINFO)printf("CmdGetRemoteHostAddress+++\r\n");
	act_size = outsize;
	tcmd->cmd_option    = CMD_ACK_KO;
	if(!BspReadHostAddress(tio)) {
        tcmd->cmd_option    = CMD_ACK_OK;
	}
    tcmd->cmd           = CMD_GET_HOST_ADDRESS;
    tcmd->cmd_index     = cmd->cmd_index;
	//if(THISINFO)printf("CmdGetRemoteHostAddress---\r\n");
    return CmdIoFinish(sock,tcmd,act_size);
}
//--------------------------------------------------------------------------------------------
//
//
int CmdSetSystemReset(TCPSOCKET * sock,CmdHead * cmd,int datasize)
{
	int act_size = 0;
	const uint8_t outsize = 0;
	uint8_t  buffer[sizeof(CmdHead)+outsize];
	CmdHead          * tcmd  = (CmdHead *)buffer;
	if(THISINFO)printf("CmdSetSystemReset.\r\n");
	act_size = outsize;
	//RequestSystemReboot();
	_ioctl(_fileno(sys_varient.resetfile), SET_RESET, NULL);
	tcmd->cmd_option    = CMD_ACK_OK;
    tcmd->cmd           = CMD_SET_SYSTEM_RESET;
    tcmd->cmd_index     = cmd->cmd_index;
    return CmdIoFinish(sock,tcmd,act_size);
}
//--------------------------------------------------------------------------------------------
//
//

const CmdMapToCmdCallType CmdCallMap[] = 
{
	{CMD_GET_IO_OUT_VALUE,CmdGetIoOutValue},
	{CMD_SET_IO_OUT_VALUE,CmdSetIoOutValue},
	{CMD_REV_IO_SOME_BIT,CmdRevertIoOutIndex},
	{CMD_GET_IO_IN_VALUE,CmdGetIoInValue},
    {CMD_SET_IP_CONFIG,CmdSetIpConfig},
    {CMD_GET_IP_CONFIG,CmdGetIpConfig},
    {CMD_GET_INPUT_CTL_MODE_INDEX,CmdGetInputCtlModeIndex},
    {CMD_SET_INPUT_CTL_MODE_INDEX,CmdSetInputCtlModeIndex},
	{CMD_GET_IO_NAME,CmdGetIoName},
	{CMD_SET_IO_NAME,CmdSetIoName},
#ifdef APP_TIMEIMG_ON
	{CMD_GET_TIMING_INFO,CmdGetIoTimingInfo},
	{CMD_SET_TIMING_INFO,CmdSetIoTimingInfo},
#endif
	{CMD_SET_RTC_VALUE,CmdSetNewRtcValue},
	{CMD_GET_RTC_VALUE,CmdGetRtcValue},
	{CMD_SET_INPUT_CTL_ON_MSK,CmdSetInputValidMsk},
	{CMD_GET_INPUT_CTL_ON_MSK,CmdGetInputValidMsk},
#ifdef APP_TIMEIMG_ON
	{CMD_SET_TIMING_ON_MSK,CmdSetTimingValidMsk},
	{CMD_GET_TIMING_ON_MSK,CmdGetTimingValidMsk},
#endif
	{CMD_SET_HOST_ADDRESS,CmdSetRemoteHostAddress},
	{CMD_GET_HOST_ADDRESS,CmdGetRemoteHostAddress},
	{CMD_SET_SYSTEM_RESET,CmdSetSystemReset},
	{0,NULL}
};

void DumpCommandRawData(void * buff,int len)
{
	int i;
	uint8_t * ptr = (uint8_t *)buff;
	if(THISINFO)printf("COMMAND DATA:");
	for(i=0;i<len;i++) {
		if(THISINFO)printf("0x%x,",*ptr++);
	}
	if(THISINFO)printf("\r\n");
}

void BinCmdPrase(TCPSOCKET * sock,void * buff,int len)
{
	uint8_t i;
	CmdHead * cmd = (CmdHead *)buff;
	if(len < sizeof(CmdHead)) {
		return ;
	}
	if(THISINFO)DumpCommandRawData(buff,len);
	i = 0;
	while(1) {
		if(CmdCallMap[i].cmd_id == cmd->cmd) {
			if(CmdCallMap[i].CmdCall) {
				(*(CmdCallMap[i].CmdCall))(sock,buff,len);
				if(THISINFO)printf("CMD finished.\r\n");
			}
			break;
		} else if(CmdCallMap[i].cmd_id == 0 && CmdCallMap[i].CmdCall == NULL) {
			if(THISERROR)printf("Prase CMD Not Found ERROR!\r\n");
			break;
		}
		++i;
	}
}

THREAD(bin_cmd_thread, arg)
{
    NutThreadSetPriority(TCP_BIN_SERVER_PRI);
    bin_cmd_thread_server();
	while(1) {
		NutSleep(100000);
	}
}

void StartBinCmdServer(void)
{
	NutThreadCreate("bin_cmd_thread",  bin_cmd_thread, 0, 2024);
}
