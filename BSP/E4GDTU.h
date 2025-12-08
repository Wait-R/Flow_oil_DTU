#ifndef __E4GDTU_H
#define __E4GDTU_H

#include "main.h"
#include <time.h>
#include "flowConfig.h"
#include "eeprom_drive.h"

typedef struct tm tm_t;

#define REV_OK		0	//接收完成标志
#define REV_WAIT	1	//接收未完成标志

uint8_t DTU_Init(void);

uint8_t DTU_Send_DataFloat(OilDataWithTime* oil_data);

uint8_t DTU_Get_NTP_Time(void);

uint8_t DTU_IsConnected_MQTT(void);

uint8_t DTU_OFF(void);

uint8_t DTU_ON(void);


#endif


