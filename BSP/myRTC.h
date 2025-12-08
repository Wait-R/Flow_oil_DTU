#ifndef __MYRTC_H
#define __MYRTC_H

#include "main.h"
#include "E4GDTU.h"

// extern time_t AsOfUnix;  // 截至时间戳
// extern time_t ReminderUnix;  // 提示时间戳

void RTC_SET_TIME(tm_t* datetime);

uint8_t RTC_GET_TIME(tm_t* datetime);

time_t convert_to_timestamp(void);

void save_RTC_Time(time_t timestamp);

time_t get_compile_timestamp(void);

// time_t Reminder_Time(void);

// time_t AsOfTime(void);


#endif


