#ifndef OUTLOG_H
#define OUTLOG_H

#include "main.h"
#include "flowConfig.h"

#define LOG_LEVEL_INIT DEFAULT_LOG_DETAIL  // 初始状态下LOG输出级别

extern char* log_Argbuffer;
extern __IO uint8_t LOG_Detailed_inf;

/* 日志级别 */
enum LOG_LEVEL {
    LOG_DEBUG = 0,
    LOG_WARN,
    LOG_ERROR,
};

void OUTLOG_Init(void);

void OUTLOG(enum LOG_LEVEL level, const char* file, const int line_number, const char* func, const char* fmt, ...);

#define  _log(x, fmt, ...) OUTLOG(x, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#endif

