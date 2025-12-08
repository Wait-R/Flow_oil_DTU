#ifndef __EEPROM_DRIVE_H
#define __EEPROM_DRIVE_H

#include "main.h"
#include "at24c64.h" // 使用AT24C128 EEPROM驱动
#include "time.h"

extern uint32_t eeprom_Count; // 存储加油次数

/* 页地址宏定义 */
#define EEPROM_START_ADDR 0x0000 /* EEPROM 起始地址 */
#define PAGE_ADDR(page_num)    ((page_num) * AT24C128_PAGE_SIZE)

/* 具体页地址 */
#define COUNT_PAGE_ADDR        PAGE_ADDR(0)    // 第0页  保存初始化标志

#define TIME_PAGE1_ADDR        PAGE_ADDR(1)    // 第1页  存储时间 

#define COEFFICIENT_PAGE_ADDR  PAGE_ADDR(2)    // 第2页  存储加油次数

#define PARAM_PAGE_NUM         PAGE_ADDR(3)    // 第3页  存储运行时参数 

// 定义油量+时间戳的存储结构体
typedef struct 
{
    float oil_flow;       // 油量数据（浮点数）
    uint32_t time_stamp;  // 秒级时间戳
} OilDataWithTime;

void AT24C128_EraseAll(void);
uint16_t eeprom_Drive_Init(void);
void x24Cxx_Write_float(uint16_t u16Addr, OilDataWithTime* pData);
void x24Cxx_Read_float(uint16_t u16Addr, OilDataWithTime *pData);
uint8_t record_Oil(OilDataWithTime oil_data);
void print_All_Data_Pages(void);
void read_Oil_Data(void);
uint16_t get_Data_Page_Count(void);
time_t Read_eeprom_Time(void);
void Write_eeprom_Time(time_t current_time);

uint8_t eeprom_read_double_from_page2(double* data);
uint8_t eeprom_erase_double_from_page2(void);
uint8_t eeprom_write_double_to_page2(double data);
void eeprom_Coefficient_Init(void);
uint8_t eeprom_increment_counter(void);
uint8_t eeprom_read_counter(uint32_t* counter);
uint8_t Auto_Update_TimeStamp(void);
uint8_t eeprom_save_device_params(void);
uint8_t eeprom_read_device_params(void);

#endif

