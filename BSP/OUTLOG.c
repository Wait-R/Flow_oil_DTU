#include "OUTLOG.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "usart.h"
#include "Task.h"
#include "app_menu.h"

#define LOG_ARG_BUFFER_SIZE 128
#define LOG_SHELL_BUFFER_SIZE 512

SemaphoreHandle_t xLOGMutex;  // 互斥锁句柄
char* log_Argbuffer = NULL; // 定义一个足够大的缓冲区
char* log_Shelltemp = NULL;
uint16_t len;
uint8_t LOG_Init_Flag = 0; // 日志初始化标志位，0-未初始化，1-已初始化
__IO uint8_t LOG_Detailed_inf = LOG_LEVEL_INIT; // 0：关闭DEBUG级别输出，1：简略DEBUG、WARN信息，2：简略DEBUG信息，3：不省略

// ANSI颜色转义码宏定义
#define ANSI_COLOR_RED     "\033[31m"      // 红色
#define ANSI_COLOR_GREEN   "\033[32m"      // 绿色
#define ANSI_COLOR_YELLOW  "\033[33m"      // 黄色
#define ANSI_COLOR_BLUE    "\033[34m"      // 蓝色
#define ANSI_COLOR_MAGENTA "\033[35m"      // 品红
#define ANSI_COLOR_CYAN    "\033[36m"      // 青色
#define ANSI_COLOR_RESET   "\033[0m"       // 重置为默认

void OUTLOG_Init(void)
{
    // 若互斥锁已存在，先释放
    if (xLOGMutex != NULL)
    {
        // 尝试获取互斥锁（防止释放时还有任务在使用）
        if (xSemaphoreTake(xLOGMutex, 100) == pdTRUE)
        {
            vSemaphoreDelete(xLOGMutex);  // 安全删除已有互斥锁
            xLOGMutex = NULL;             // 重置为NULL
            LOG_Init_Flag = 0; // 设置初始化完成标志
        }
        else
        {
            printf("LOG Mutex is in use, cannot reinitialize!\r\n");
            return;  // 互斥锁正被使用，暂时无法重初始化
        }
    }
    xLOGMutex = xSemaphoreCreateMutex();
    if (xLOGMutex == NULL) {
        // 互斥锁创建失败，处理错误
        printf("LOG Mutex Creation Failed!\r\n");
        Error_Handler();
    }
    log_Shelltemp = pvPortMalloc(sizeof(char) * LOG_SHELL_BUFFER_SIZE);
    log_Argbuffer = pvPortMalloc(sizeof(char) * LOG_ARG_BUFFER_SIZE);
    if(log_Shelltemp == NULL || log_Argbuffer == NULL) {
        // 内存申请失败，处理错误
        printf("Log Buffer Creation Failed!\r\n");
        Error_Handler();
    }
    memset(log_Shelltemp, 0, sizeof(char) * LOG_SHELL_BUFFER_SIZE);
    memset(log_Argbuffer, 0, sizeof(char) * LOG_ARG_BUFFER_SIZE);
    LOG_Init_Flag = 1; // 设置初始化完成标志
}

void OUTLOG(enum LOG_LEVEL level, const char *file, const int line_number, const char *func, const char *fmt, ...)
{
    if(LOG_Init_Flag == 0) {
        // 日志系统未初始化，直接返回
        printf("LOG System Not Init!\r\n");
        return;
    }
    if(led_state == 3) {
        xTaskNotify(defaultTaskHandle, EVENTBIT_0, eSetBits);
    }
    else if(led_state == 4 && level == LOG_ERROR)
    {
        xTaskNotify(defaultTaskHandle, EVENTBIT_1, eSetBits);
    }

    if(xSemaphoreTake(xLOGMutex, pdMS_TO_TICKS(500)) == pdTRUE) {  // 获取资源
        if(level == LOG_DEBUG && LOG_Detailed_inf == 0) {
            xSemaphoreGive(xLOGMutex); // 释放资源
            return; // 如果是调试级别且调试日志被禁用，则直接返回
        }

        va_list args;
        va_start(args, fmt);
        vsnprintf(log_Argbuffer, LOG_ARG_BUFFER_SIZE, fmt, args); // 使用可变参数格式化字符串
        va_end(args);

        TaskHandle_t xCurrentTask = xTaskGetCurrentTaskHandle();  // 获取当前任务句柄
        const char* pcTaskName = "NoTask";     // 当前任务名称
        if(xCurrentTask != NULL) {
            pcTaskName = pcTaskGetName(xCurrentTask);     // 获取当前任务名称
        }

        switch(level)
        {
            case LOG_DEBUG:
                if(LOG_Detailed_inf == 1 || LOG_Detailed_inf == 2) {
                    len = sprintf(log_Shelltemp, "** %s[Info]:{ %s } %s**\r\n",
                        ANSI_COLOR_GREEN, log_Argbuffer, ANSI_COLOR_RESET);
                }
                else {
                    len = sprintf(log_Shelltemp, "** %slog: [Task:%s]-[Time:%lu]-[FILE:%s]-[LINE:%d]-[func:%s] %s**\r\n** %s[Info]:{ %s } %s**\r\n",
                        ANSI_COLOR_GREEN, pcTaskName, HAL_GetTick(), file, line_number, func, ANSI_COLOR_RESET, ANSI_COLOR_GREEN, log_Argbuffer, ANSI_COLOR_RESET);
                }
                break;
            case LOG_WARN:
                if(LOG_Detailed_inf == 1) {
                    len = sprintf(log_Shelltemp, "** %s[Info]:{ %s } %s**\r\n",
                        ANSI_COLOR_YELLOW, log_Argbuffer, ANSI_COLOR_RESET);
                }
                else {
                    len = sprintf(log_Shelltemp, "** %slog: [Task:%s]-[Time:%lu]-[FILE:%s]-[LINE:%d]-[func:%s] %s**\r\n** %s[Info]:{ %s } %s**\r\n",
                        ANSI_COLOR_YELLOW, pcTaskName, HAL_GetTick(), file, line_number, func, ANSI_COLOR_RESET, ANSI_COLOR_YELLOW, log_Argbuffer, ANSI_COLOR_RESET);
                }
                break;
            case LOG_ERROR:
                len = sprintf(log_Shelltemp, "** %slog: [Task:%s]-[Time:%lu]-[FILE:%s]-[LINE:%d]-[func:%s] %s**\r\n** %s[Info]:{ %s } %s**\r\n", 
                    ANSI_COLOR_RED, pcTaskName, HAL_GetTick(), file, line_number, func, ANSI_COLOR_RESET, ANSI_COLOR_RED, log_Argbuffer, ANSI_COLOR_RESET);
                break;
            default:
                break;
        }
        HAL_UART_Transmit(&huart1, (const uint8_t *)log_Shelltemp, len, 20);
        xSemaphoreGive(xLOGMutex); // 释放资源
    }
}


