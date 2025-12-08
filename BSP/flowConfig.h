#ifndef __FLOWCONFIG_H
#define __FLOWCONFIG_H

/***********************************************************
需要修改：
油罐车序号（需要分配好）
***********************************************************/

/* 油罐车序号 */
#define SERIAL_NUMBER 4


/* ESP8266 初始化超时重试次数 */
#define DF_TIMEOUT 2

/* ESP8266 接收数据超时时间（ms 10的倍数） */
#define DF_RECIVE_TIMEOUT 10000

/* 连续多少秒数据不变上传数据 (S 整数) */
#define DF_UNCHANG_TIME  5

/* 最小发送加油量 */
#define DF_MIN_FLOW  1.0f

/* 发送MQTT消息的最小延时(ms) */
#define DF_DELAY_SEND  10

#endif



