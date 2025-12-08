#include "Task.h"
#include "flowConfig.h"
#include <stdio.h>
#include <math.h>
#include "cmsis_os.h"
#include "queue.h"
#include "myRTC.h"
#include "eeprom_drive.h"
#include "delay.h"
#include "iwdg.h"
#include "E4GDTU.h"
#include "OLED.h"
#include "Key.h"
#include "stm32f1xx_hal_rtc.h"

uint8_t upload_pending = 0;  /* 开始加油标志 */

/* LED灯闪烁 */
void LED_Task(void const* argument)
{
  HAL_IWDG_Init(&hiwdg);
	
  for(;;) {
    LED_ON();
    vTaskDelay(300);
    LED_OFF();
    vTaskDelay(4700);
    HAL_IWDG_Refresh(&hiwdg); // 喂狗
  }
}

/* 上传数据 */
void UploadData_Task(void const * argument)
{
  uint32_t notify_value = 0; /* 任务通知值 */
  OilDataWithTime read_data; /* 读取的油量+时间戳数据结构体 */

  if(DTU_Init() == OK) {
    printf("DTU模块初始化成功\r\n");
  }
  else {
    printf("DTU模块初始化失败\r\n");
  }
  printf("当前RTC时间戳：%ld\r\n", convert_to_timestamp());
  vTaskDelay(500);

  if(convert_to_timestamp() < get_compile_timestamp()) { /* 如果当前时间小于编译时间-进行校准 */
    printf("RTC时间异常，正在校准RTC时间...\r\n");
    if(DTU_Get_NTP_Time()==OK) {
      printf("RTC时间校准成功\r\n");
    }
    else {
      printf("RTC时间校准失败\r\n");
    }
  }

  printf("当前RTC时间戳：%ld\r\n", convert_to_timestamp());

  for(;;)
  {
    xTaskNotifyWait(0x00, EVENTBIT_0, &notify_value, 0);
    if(notify_value & EVENTBIT_0) {
      // 执行校准时间函数
      if(DTU_Get_NTP_Time()==OK) {
        printf("RTC时间校准成功\r\n");
      }
      else {
        printf("RTC时间校准失败\r\n");
        
      }
    }

    if(get_Data_Page_Count() > 0) {
      DTU_ON();
      if(DTU_IsConnected_MQTT() == NO) {
        printf("DTU模块MQTT未连接，正在重新连接...\r\n");
        if(DTU_Init() == OK) {
          printf("DTU模块重新连接成功\r\n");
        }
        else {
          printf("DTU模块重新连接失败\r\n");
        }
      }
      else {
        if(xQueueSend(RequestQueueHandle,&notify_value,0) != pdPASS) {
			    printf("错误：无法申请数据\r\n");
        }
        if(xQueueReceive(UploadQueueHandle, &read_data, 10000) == pdPASS) {
          if(DTU_Send_DataFloat(&read_data) == OK) {
          }
          else {
            printf("数据上传失败\r\n");
            if(DTU_Init() == OK) {
              printf("DTU模块重新连接成功\r\n");
              vTaskDelay(10000);
            }
          }
        }
      }
    }
    else {
      DTU_OFF();
      vTaskDelay(10000);
    }
  }

}

/* 处理数据 */
void ProcesData_Task(void const * argument)
{
  float result = 0; /* 当前油量 */
  float last_result = 0; /* 上一次油量 */
  uint8_t unchanged_count = 0; /* 静止加油标志 */

  last_result = Get_Traffic();
  printf("首次启动，仅记录初始流量 %.3f L,不上传\r\n", result);
  

  // for (int16_t i = 0; i < 10; i++)
  // {
  //   vTaskDelay(20000);
  //   result = 101 + 0.1 * i; // 模拟初始流量
  //   xQueueSend(SendQueueHandle, &result, 2000); // 发送测试
  // }

  for(;;)
  {   
    result = Get_Traffic();
    // printf("当前流量: %.3f L\r\n", result);
    if (fabs(result - last_result) > 0.001) {
      if (!upload_pending) {
        upload_pending = 1;  // 标记开始加油
        printf("开始加油，累计值为 %.3f\r\n", result);
      }
      unchanged_count = 0;  // 重置静止计数器
      last_result = result;
    }
    else if (upload_pending) {
      unchanged_count++;
      printf("开始加油，累计值为 %.3f\r\n", result);
      printf("加油值连续未变 %d/%d 秒\r\n", unchanged_count, DF_UNCHANG_TIME);
      if (unchanged_count >= DF_UNCHANG_TIME) {
        // 加油完成，上传当前累计值 result
        if (result > DF_MIN_FLOW)  //添加这个判断：过滤掉接近0的流量值
        {
          if(xQueueSend(SendQueueHandle, &result, 1000000) != pdPASS) {
            printf("错误：数据无法发送到队列，已超时或队列满\r\n");
          }
          printf("本次加油流量为%.3f\r\n",result);
        }
        else {
          printf("本次加油流量为0,取消上传\r\n");
        }
        Clear_Traffic();        // 重置油量计数
        upload_pending = 0;     // 退出加油状态，防止继续上传0
        unchanged_count = 0;
        last_result = 0;             //重置累计值起点
      }
    }
    vTaskDelay(1000);
  }
}

/* 存储数据 */
void StorageData_Task(void const * argument)
{
  // AT24C128_EraseAll(); // 擦除整个EEPROM

  OilDataWithTime oil_data; /* 需要保存的油量+时间戳结构体 */

  uint8_t eeprom_flag = AT24C128_ReadByte(COUNT_PAGE_ADDR);
  if(eeprom_flag != 0xA5) {
    printf("EEPROM 未初始化，正在初始化...\r\n");
    AT24C128_EraseAll(); // 擦除整个EEPROM
    AT24C128_WriteByte(COUNT_PAGE_ADDR, 0xA5); // 标记已初始化
    vTaskDelay(100);
  }
  else {
    printf("EEPROM 已初始化\r\n");
  }
  printf("eeprom data count:%d\r\n", eeprom_Drive_Init());

  print_All_Data_Pages();

  float receivedValue = 0; /* 需要保存的油量 */

  for(;;)
  {
    if(xQueueReceive(SendQueueHandle, &receivedValue, 0) == pdPASS) {
      oil_data.time_stamp = convert_to_timestamp();

      oil_data.oil_flow = receivedValue;
      switch(record_Oil(oil_data)) {
        case 0:
          printf("流量保存成功\r\n");
          break;
        case 1:
          printf("EEPROM 已满\r\n");
          break;
        default:
          printf("写入错误\r\n");
          break;
      }
      delay_ms(5);
    }

    if(get_Data_Page_Count() > 0) {
      UBaseType_t queue_space = uxQueueSpacesAvailable(UploadQueueHandle);
      if(queue_space > 0) // 队列有空闲
      {
        read_Oil_Data(); // 读取数据
      }
    }
    vTaskDelay(500);
  }
}

/*oled 显示任务*/
void oled_Task(void const* argument)
{
  uint8_t oled_Mode = 1;  // 记录OLED显示模式
  uint8_t oled_Init_Flag = 0; // OLED初始化标志
  char time_str[20];      // 时间字符串缓存
  char eeprom_str[20];    // EEPROM占用字符串缓存
  float current_Max_Flow = 0.0f; // 历史最大流量值

  for(;;)
  {
    // printf(".");
    if(Key_Check(KEY0, KEY_LONG)) {
      /* 长按按键 启动/关闭 OLED */
      oled_Mode++;
      if(oled_Mode > 1) {
        oled_Mode = 0;
      }
    }

    if(Key_Check(KEY0, KEY_SINGLE)) {
      /* 单击按键 清除历史数据 */
      current_Max_Flow = 0;
    }

    if(Key_Check(KEY0, KEY_DOUBLE)) {
      /* 双击按键 重新校准时间 */
      xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知上传任务校准时间
    }

    if(oled_Mode == 0) {
      /* 不显示 */
      if(oled_Init_Flag == 1) {
        OLED_Clear();
        OLED_Update();
        oled_Init_Flag = 0;
      }
      vTaskDelay(200);
    }
    else if(oled_Mode == 1) {
      /* 显示模式：初始化OLED（首次开启时）并更新显示内容 */
      if(oled_Init_Flag == 0) {
        OLED_Init();
        OLED_Clear();
        OLED_Update();
        oled_Init_Flag = 1;
      }
      /* 显示当前流量 */
      float current_flow = (float)Get_Traffic();
      /* 计算历史流量最大值 */
      if(current_flow > current_Max_Flow) {
        current_Max_Flow = current_flow;
      }
      /* 显示当前空间使用情况 */
      uint16_t used_count = get_Data_Page_Count();  // 已存储数据个数
      uint16_t total_count = 2016;                  // 最大可存储数据个数
      uint8_t usage_rate = (used_count * 100) / total_count;  // 占用率（%）
      sprintf(eeprom_str, "EEPROM: %d/%d (%d%%)", used_count, total_count, usage_rate);
      /* 显示时间 */
      tm_t current_time;
      RTC_GET_TIME(&current_time);
      sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
              current_time.tm_year + 1900,  // 年份修正
              current_time.tm_mon + 1,     // 月份修正（0-11→1-12）
              current_time.tm_mday,
              current_time.tm_hour,
              current_time.tm_min,
        current_time.tm_sec);

      /* 显示内容更新（每行16像素高，8x16字体）*/
      // 第一行：当前油量（保留2位小数）
      OLED_Printf(0, 0, OLED_8X16, "Flow: %.2f L      ", current_flow);
      // 第二行：历史最大油量（保留2位小数）
      OLED_Printf(0, 16, OLED_8X16, "Max: %.2f L       ", current_Max_Flow);
      // 第三行：EEPROM占用情况
      OLED_Printf(0, 32, OLED_6X8, "%s", eeprom_str);
      // 第四行：当前时间
      OLED_Printf(0, 40, OLED_6X8, "%s", time_str);
      // 第五行：累计加油次数
      OLED_Printf(0, 48, OLED_6X8, "Count:%d", eeprom_Count);
      /* 刷新OLED显示 */
      OLED_Update();

      vTaskDelay(1000); // 每秒更新一次
    }
  }
}
