#ifndef __MULTIMGR_DEVICE_H__
#define __MULTIMGR_DEVICE_H__
#include "multimgr_device_dev.h"

extern device_info_st    multimgr_info;

void StratMultiMgrDeviceThread(void);

#define BROADCASTTIME_MIN      5
#define BROADCASTTIME_MAX      60


#endif


