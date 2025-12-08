#include "myRTC.h"
#include "rtc.h"
#include "string.h"
#include <stdio.h>

// time_t AsOfUnix = 0;  // 截至时间戳
// time_t ReminderUnix = 0;  // 提示时间戳



/**
 * @brief  设置RTC时间
 * @param  datetime: 指向tm结构体的指针（需符合标准：tm_mon 0-11，tm_year为年份-1900）
 * @note   RTC的月份范围是1-12，年份是从2000年开始的偏移（0-99）
 */
void RTC_SET_TIME(tm_t* datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    sTime.Hours = datetime->tm_hour;
    sTime.Minutes = datetime->tm_min;
    sTime.Seconds = datetime->tm_sec;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler(); 
    }

    sDate.Year = (datetime->tm_year + 1900) - 2000;  // tm_year是相对于1900的偏移，转换为RTC的2000年偏移（0-99）
    sDate.Month = datetime->tm_mon + 1;              // tm_mon 0-11 -> RTC Month 1-12
    sDate.Date = datetime->tm_mday; 
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
 * @brief  获取RTC时间
 * @param  datetime: 指向tm结构体的指针（输出结果：tm_mon 0-11，tm_year为年份-1900）
 * @retval 0: 成功，1: 失败
 */
uint8_t RTC_GET_TIME(tm_t* datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // 必须先读时间，再读日期（HAL库要求，否则可能获取到旧值）
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return NO; 
    }
    
    if(HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return NO;  
    }

    // 转换为tm结构体
    datetime->tm_year = (2000 + sDate.Year) - 1900;  // RTC年份（2000+偏移）转换为相对于1900的偏移
    datetime->tm_mon = sDate.Month - 1;              // RTC Month 1-12 -> tm_mon 0-11
    datetime->tm_mday = sDate.Date;
    datetime->tm_hour = sTime.Hours;
    datetime->tm_min = sTime.Minutes;
    datetime->tm_sec = sTime.Seconds;
    datetime->tm_isdst = 0;                          // 禁用夏令时

    return 0;  // 成功
}

/**
 * @brief 获取当前系统时间戳
 * @param 
 * @return 自1970年1月1日起的Unix时间戳
 */
time_t convert_to_timestamp(void)
{
    tm_t timeinfo = {0};
    // 获取时间信息
		RTC_GET_TIME(&timeinfo);
    // 转换为时间戳
    return mktime(&timeinfo) - 8 * 3600; // 减去8小时的时区偏移
}

/**
 * @brief 根据时间戳保存RTC时间
 * @param 
 * @return 自1970年1月1日起的Unix时间戳
 */
void save_RTC_Time(time_t timestamp)
{
	tm_t *timeinfo;
	timeinfo = gmtime(&timestamp); // 获取UTC时间结构体

	RTC_SET_TIME(timeinfo); // 设置RTC时间
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
