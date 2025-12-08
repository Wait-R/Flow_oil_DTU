#include "myRTC.h"
#include "rtc.h"
#include "string.h"
#include <stdio.h>

// time_t AsOfUnix = 0;  // 截至时间戳
// time_t ReminderUnix = 0;  // 提示时间戳

// RTC已经被初始化的值 记录在RTC_BKP_DR1中
#define RTC_INIT_FLAG 0x2333

/**
 * @brief  进入RTC初始化模式
 * @param  hrtc  指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef *hrtc)
{
  uint32_t tickstart = 0U;

  tickstart = HAL_GetTick();
  /* 等待RTC处于INIT状态，如果到达Time out 则退出 */
  while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if ((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
      return HAL_TIMEOUT;
    }
  }

  /* 禁用RTC寄存器的写保护 */
  __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);


  return HAL_OK;
}

/**
 * @brief  退出RTC初始化模式
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef *hrtc)
{
  uint32_t tickstart = 0U;

  /* 禁用RTC寄存器的写保护。 */
  __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

  tickstart = HAL_GetTick();
  /* 等到RTC处于INIT状态，如果到达Time out 则退出 */
  while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
  {
    if ((HAL_GetTick() - tickstart) >  RTC_TIMEOUT_VALUE)
    {
      return HAL_TIMEOUT;
    }
  }

  return HAL_OK;
}

/**
 * @brief  写入RTC_CNT寄存器中的时间计数器。
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针。
 * @param  TimeCounter: 写入RTC_CNT寄存器的计数器
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_WriteTimeCounter(RTC_HandleTypeDef *hrtc, uint32_t TimeCounter)
{
  HAL_StatusTypeDef status = HAL_OK;

  /* 进入RTC初始化模式 */
  if (RTC_EnterInitMode(hrtc) != HAL_OK)
  {
    status = HAL_ERROR;
  }
  else
  {
	/* 设置RTC计数器高位寄存器 */
    WRITE_REG(hrtc->Instance->CNTH, (TimeCounter >> 16U));
    /* 设置RTC计数器低位寄存器 */
    WRITE_REG(hrtc->Instance->CNTL, (TimeCounter & RTC_CNTL_RTC_CNT));

    /* 退出RTC初始化模式 */
    if (RTC_ExitInitMode(hrtc) != HAL_OK)
    {
      status = HAL_ERROR;
    }
  }

  return status;
}


/**
 * @brief  读取RTC_CNT寄存器中的时间计数器。
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针。
 * @retval 时间计数器
 */
static uint32_t RTC_ReadTimeCounter(RTC_HandleTypeDef *hrtc)
{
  uint16_t high1 = 0U, high2 = 0U, low = 0U;
  uint32_t timecounter = 0U;

  high1 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
  low   = READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
  high2 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);

  if (high1 != high2)
  {
	/* 当读取CNTL和CNTH寄存器期间计数器溢出时, 重新读取CNTL寄存器然后返回计数器值 */
    timecounter = (((uint32_t) high2 << 16U) | READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT));
  }
  else
  {
	/* 当读取CNTL和CNTH寄存器期间没有计数器溢出, 计数器值等于第一次读取的CNTL和CNTH值 */
    timecounter = (((uint32_t) high1 << 16U) | low);
  }

  return timecounter - 8 * 3600; // 减去8小时的时区偏移
}

/**
 * @brief 设置RTC时间
 * @param time 时间
 * @retval HAL status
 */
HAL_StatusTypeDef KK_RTC_SetTime(struct tm *time){
	uint32_t unixTime = mktime(time) + 2;
	return RTC_WriteTimeCounter(&hrtc, unixTime);
}

/**
 * @brief 获取RTC时间
 * @retval 时间
 */
struct tm *KK_RTC_GetTime(void) {
    time_t unixTime = RTC_ReadTimeCounter(&hrtc);
//		_log(LOG_DEBUG,"RTC Read TimeCounter: %ld\r\n", unixTime);
    tm_t* timeinfo;
    timeinfo = gmtime(&unixTime); // 获取UTC时间结构体
    return timeinfo;
}

void KK_RTC_Init(void){
	uint32_t initFlag = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
	if(initFlag == RTC_INIT_FLAG) return;
	if (HAL_RTC_Init(&hrtc) != HAL_OK){
		Error_Handler();
	}
	struct tm time = {
		  .tm_year = 2025 - 1900,
		  .tm_mon = 1 - 1,
		  .tm_mday = 1,
		  .tm_hour = 23,
		  .tm_min = 59,
		  .tm_sec = 55,
	};
	KK_RTC_SetTime(&time);
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_INIT_FLAG);

}


/**
 * @brief  设置RTC时间
 * @param  datetime: 指向tm结构体的指针（需符合标准：tm_mon 0-11，tm_year为年份-1900）
 * @note   RTC的月份范围是1-12，年份是从2000年开始的偏移（0-99）
 */
void RTC_SET_TIME(tm_t* datetime)
{
    // RTC_TimeTypeDef sTime = {0};
    // RTC_DateTypeDef sDate = {0};

    // sTime.Hours = datetime->tm_hour;
    // sTime.Minutes = datetime->tm_min;
    // sTime.Seconds = datetime->tm_sec;
    // if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    // {
    //     Error_Handler(); 
    // }

    // sDate.Year = (datetime->tm_year + 1900) - 2000;  // tm_year是相对于1900的偏移，转换为RTC的2000年偏移（0-99）
    // sDate.Month = datetime->tm_mon + 1;              // tm_mon 0-11 -> RTC Month 1-12
    // sDate.Date = datetime->tm_mday; 
    // if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    // {
    //     Error_Handler();
    // }
    KK_RTC_SetTime( datetime );
}


/**
 * @brief  获取RTC时间
 * @param  datetime: 指向tm结构体的指针（输出结果：tm_mon 0-11，tm_year为年份-1900）
 * @retval 0: 成功，1: 失败
 */
uint8_t RTC_GET_TIME(tm_t* datetime)
{
  // RTC_TimeTypeDef sTime = {0};
  // RTC_DateTypeDef sDate = {0};

  // // 必须先读时间，再读日期（HAL库要求，否则可能获取到旧值）
  // if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  // {
  //     return NO; 
  // }
  
  // if(HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  // {
  //     return NO;  
  // }

  // // 转换为tm结构体
  // datetime->tm_year = (2000 + sDate.Year) - 1900;  // RTC年份（2000+偏移）转换为相对于1900的偏移
  // datetime->tm_mon = sDate.Month - 1;              // RTC Month 1-12 -> tm_mon 0-11
  // datetime->tm_mday = sDate.Date;
  // datetime->tm_hour = sTime.Hours;
  // datetime->tm_min = sTime.Minutes;
  // datetime->tm_sec = sTime.Seconds;
  // datetime->tm_isdst = 0;                          // 禁用夏令时
  datetime = KK_RTC_GetTime();

  return 0;  // 成功
}

/**
 * @brief 获取当前系统时间戳
 * @param 
 * @return 自1970年1月1日起的Unix时间戳
 */
time_t convert_to_timestamp(void)
{
  return RTC_ReadTimeCounter(&hrtc);
}

/**
 * @brief 根据时间戳保存RTC时间
 * @param 
 * @return 自1970年1月1日起的Unix时间戳
 */
void save_RTC_Time(time_t timestamp)
{
	// tm_t *timeinfo;
	// timeinfo = gmtime(&timestamp); // 获取UTC时间结构体

    // RTC_SET_TIME(timeinfo); // 设置RTC时间   
    RTC_WriteTimeCounter(&hrtc, timestamp);
}

// /**
//  * @brief 获取提示日期时间戳
//  * @param 
//  * @return 返回提示日期时间戳
//  */
// time_t Reminder_Time(void)
// {
// 	struct tm timeinfo = { 0 };
	
// 	// 设置时间信息
//     timeinfo.tm_year = DF_HINT_EXPIRATION_TIME_YEAR - 1900;  // 年份减去1900
//     timeinfo.tm_mon = DF_HINT_EXPIRATION_TIME_MONTH - 1;     // 月份从0开始，所以减1
//     timeinfo.tm_mday = DF_HINT_EXPIRATION_TIME_DAY;
//     timeinfo.tm_hour = 0;
//     timeinfo.tm_min = 0;
//     timeinfo.tm_sec = 0;
    
//     // 转换为时间戳
//     return mktime(&timeinfo) - 8 * 3600;
// }

// /**
//  * @brief 获取截至日期时间戳
//  * @param 
//  * @return 返回截至日期时间戳
//  */
// time_t AsOfTime(void)
// {
// 	struct tm timeinfo = { 0 };
	
// 	// 设置时间信息
//     timeinfo.tm_year = DF_EXPIRATION_TIME_YEAR - 1900;  // 年份减去1900
//     timeinfo.tm_mon = DF_EXPIRATION_TIME_MONTH - 1;     // 月份从0开始，所以减1
//     timeinfo.tm_mday = DF_EXPIRATION_TIME_DAY;
//     timeinfo.tm_hour = 0;
//     timeinfo.tm_min = 0;
//     timeinfo.tm_sec = 0;
    
//     // 转换为时间戳
//     return mktime(&timeinfo) - 8 * 3600;
// }

// 月份字符串转数字（Jan=0, Feb=1, ..., Dec=11）
static int month_str_to_num(const char *month_str) {
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strcmp(month_str, months[i]) == 0) {
            return i;
        }
    }
    return -1; // 无效月份
}

/**
 * @brief 将编译时间转换为Unix时间戳（秒）
 * @param NULL
 * @return 返回截至日期时间戳
 */
time_t get_compile_timestamp(void)
{
    struct tm compile_time = {0};
    char date_str[12];  // 存储__DATE__字符串
    char time_str[9];   // 存储__TIME__字符串
    int month, day, year, hour, minute, second;

    // 获取编译日期和时间（编译器宏）
    strncpy(date_str, __DATE__, sizeof(date_str)-1);
    strncpy(time_str, __TIME__, sizeof(time_str)-1);

    // 解析日期（格式："MMM DD YYYY"）
    char month_str[4];
    sscanf(date_str, "%3s %d %d", month_str, &day, &year);
    month = month_str_to_num(month_str);
    if (month == -1) {
        return 0; // 解析失败
    }

    // 解析时间（格式："HH:MM:SS"）
    sscanf(time_str, "%d:%d:%d", &hour, &minute, &second);

    // 填充tm结构体（注意：tm_mon从0开始，tm_year为年份-1900）
    compile_time.tm_sec = second;
    compile_time.tm_min = minute;
    compile_time.tm_hour = hour;
    compile_time.tm_mday = day;
    compile_time.tm_mon = month;
    compile_time.tm_year = year - 1900; // tm_year是自1900年的偏移量
    compile_time.tm_isdst = -1; // 不考虑夏令时

    // 转换为时间戳（秒）
    return mktime(&compile_time) - 8 * 3600; // 减去8小时的时区偏移
}

// 内部辅助函数声明
static int is_leap_year(int year);
static int get_days_in_month(int year, int month);
static void parse_timestamp(long long timestamp, int timezone_offset, int *year, int *month, int *day, int *hour, int *minute, int *second); 

/**
 * @brief 获取时间戳对应的年份
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时），例如北京时间为 +8
 * @return 年份
 */
int get_year(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return year;
}

/**
 * @brief 获取时间戳对应的月份
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @return 月份（1-12）
 */
int get_month(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return month;
}

/**
 * @brief 获取时间戳对应的日期
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @return 日期（1-31）
 */
int get_day(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return day;
}

/**
 * @brief 获取时间戳对应的小时
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @return 小时（0-23）
 */
int get_hour(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return hour;
}

/**
 * @brief 获取时间戳对应的分钟
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @return 分钟（0-59）
 */
int get_minute(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return minute;
}

/**
 * @brief 获取时间戳对应的秒数
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @return 秒数（0-59）
 */
int get_second(long long timestamp, int timezone_offset) {
    int year, month, day, hour, minute, second;
    parse_timestamp(timestamp, timezone_offset, &year, &month, &day, &hour, &minute, &second);
    return second;
}

/**
 * @brief 内部辅助函数：解析时间戳，计算出年、月、日、时、分、秒
 * @param timestamp 时间戳
 * @param timezone_offset 时区偏移量（单位：小时）
 * @param year 输出参数：年份
 * @param month 输出参数：月份（1-12）
 * @param day 输出参数：日期（1-31）
 * @param hour 输出参数：小时（0-23）
 * @param minute 输出参数：分钟（0-59）
 * @param second 输出参数：秒数（0-59）
 */
static void parse_timestamp(long long timestamp, int timezone_offset, int *year, int *month, int *day, int *hour, int *minute, int *second) {
    // 根据时区偏移量调整时间戳
    timestamp += timezone_offset * 3600;

    *second = timestamp % 60;
    *minute = (timestamp / 60) % 60;
    *hour = (timestamp / 3600) % 24;
    long long days = timestamp / 86400;

    *year = 1970;
    while (1) {
        int days_in_year = is_leap_year(*year) ? 366 : 365;
        if (days < days_in_year) {
            break;
        }
        days -= days_in_year;
        (*year)++;
    }

    *month = 1;
    while (*month <= 12) {
        int days_in_month = get_days_in_month(*year, *month);
        if (days < days_in_month) {
            break;
        }
        days -= days_in_month;
        (*month)++;
    }

    *day = days + 1;
}

/**
 * @brief 内部辅助函数：判断是否为闰年
 * @param year 年份
 * @return 1 表示闰年，0 表示平年
 */
static int is_leap_year(int year) {
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    if (year % 4 == 0) return 1;
    return 0;
}

/**
 * @brief 内部辅助函数：获取指定年份和月份的天数
 * @param year 年份
 * @param month 月份（1-12）
 * @return 该月的天数
 */
static int get_days_in_month(int year, int month) {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}


