#ifndef __TASK_H
#define __TASK_H

#include "FreeRTOS.h"
#include "led.h"
#include "task.h"
#include "timers.h"
#include "main.h"
#include "usart.h"
#include "E4GDTU.h"
#include "cmsis_os.h"
#include "Capture_Traffic.h"
#include "OUTLOG.h"

#define EVENTBIT_0      (1 << 0)            /* 事件位 */
#define EVENTBIT_1      (1 << 1)
#define EVENTBIT_2      (1 << 2)
#define EVENTBIT_3      (1 << 3)
#define EVENTBIT_4      (1 << 4)
#define EVENTBIT_5      (1 << 5)

extern osThreadId defaultTaskHandle;
extern osThreadId UploadDataTaskHandle;
extern osThreadId ProcesDataTaskHandle;
extern osThreadId StorageDataTaskHandle;
extern osThreadId oledTaskHandle;
extern osMessageQId SendQueueHandle;    /* 传递存储队列句柄 */
extern osMessageQId UploadQueueHandle;  /* 上传数据队列句柄 */
extern osMessageQId RequestQueueHandle; /* 请求数据队列句柄 */
extern osMessageQId DeleteQueueHandle;  /* 删除数据队列句柄 */

void E4GDTU_MODE(void);
void ESP8266_MODE(void);
#endif




