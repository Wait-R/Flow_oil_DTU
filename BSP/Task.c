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
#include "app_menu.h"

uint8_t upload_pending = 0;  /* 开始加油标志 */

/* 上传数据 */
void UploadData_Task(void const * argument)
{
  uint32_t notify_value = 0; /* 任务通知值 */
  OilDataWithTime read_data; /* 读取的油量+时间戳数据结构体 */

  xTaskNotifyWait(0x00, EVENTBIT_0 | EVENTBIT_1, &notify_value, 5000);

  if(notify_value & EVENTBIT_1) {
    _log(LOG_WARN, "DTU NO Init");
    if(DTU_Init() == OK) {
      _log(LOG_DEBUG,"DTU模块初始化成功");
    }
    else {
      _log(LOG_ERROR,"DTU模块初始化失败");
    }
  }
  
  if(convert_to_timestamp() < get_compile_timestamp()) { /* 如果当前时间小于编译时间-进行校准 */
    _log(LOG_WARN, "RTC时间异常，准备RTC时间...");
    xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知上传任务校准时间
  }
  else {
    _log(LOG_DEBUG,"当前RTC时间戳：%ld", convert_to_timestamp());
  }
  
  for(;;)
  {
    xTaskNotifyWait(0x00, EVENTBIT_0 | EVENTBIT_1, &notify_value, 0);
    if(notify_value & EVENTBIT_0) {
      _log(LOG_DEBUG, "收到校准时间通知，正在校准RTC时间...");
      DTU_ON();
      vTaskDelay(10000);
      // 执行校准时间函数
      if(DTU_Get_NTP_Time()==OK) {
        _log(LOG_DEBUG, "RTC时间校准成功");
      }
      else {
        _log(LOG_WARN, "RTC时间校准失败");
      }
    }

    if(get_Data_Page_Count() > 0) {
      DTU_ON();
      if(DTU_IsConnected_MQTT() == NO || UPdata_UP_flag == 0) {
        _log(LOG_WARN, "DTU模块MQTT未连接，正在重新连接...");
        if(DTU_Init() == OK) {
          _log(LOG_DEBUG, "DTU模块重新连接成功");
        }
        else {
          _log(LOG_ERROR, "DTU模块重新连接失败");
        }
      }
      else {
        _log(LOG_DEBUG, "DTU模块MQTT已连接，准备上传数据...");
        if(xQueueSend(RequestQueueHandle, &notify_value, 0) != pdPASS) {
          _log(LOG_ERROR, "错误：无法申请数据");
        }
        else {
          _log(LOG_DEBUG, "已请求读取数据");
        }
        if(xQueueReceive(UploadQueueHandle, &read_data, 10000) == pdPASS) {
          if(DTU_Send_DataFloat(&read_data) == OK) {
          }
          else {
            _log(LOG_WARN, "数据上传失败");
            if(DTU_Init() == OK) {
              _log(LOG_DEBUG, "DTU模块重新连接成功");
              vTaskDelay(10000);
            }
          }
        }
      }
    }
    else {
      DTU_OFF();
      vTaskDelay(5000);
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
  _log(LOG_DEBUG, "首次启动，仅记录初始流量 %.3f L,不上传", last_result);

  // for (int16_t i = 0; i < 3; i++)
  // {
  //   result = 101 + 0.1 * i; // 模拟初始流量
  //   xQueueSend(SendQueueHandle, &result, 2000); // 发送测试
  // }

  for(;;)
  {   
    result = Get_Traffic();
    // _log(LOG_DEBUG, "当前流量: %.3f L", result);
    if (fabs(result - last_result) > 0.001) {
      if (!upload_pending) {
        upload_pending = 1;  // 标记开始加油
        _log(LOG_DEBUG, "开始加油，累计值为 %.3f L", result);
      }
      unchanged_count = 0;  // 重置静止计数器
      last_result = result;
    }
    else if (upload_pending) {
      unchanged_count++;
      _log(LOG_DEBUG, "开始加油，累计值为 %.3f", result);
      _log(LOG_DEBUG, "加油值连续未变 %d/%d 秒", unchanged_count, DF_UNCHANG_TIME);
      if (unchanged_count >= DF_UNCHANG_TIME) {
        // 加油完成，上传当前累计值 result
        if (result > DF_MIN_FLOW)  //添加这个判断：过滤掉接近0的流量值
        {
          if(xQueueSend(SendQueueHandle, &result, 1000000) != pdPASS) {
            _log(LOG_ERROR, "错误：数据无法发送到队列，已超时或队列满");
          }
          _log(LOG_DEBUG,"本次加油流量为%.3f", result);
        }
        else {
          _log(LOG_DEBUG,"本次加油流量为0,取消上传");
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

  OUTLOG_Init(); // 初始化日志模块

  OilDataWithTime oil_data; /* 需要保存的油量+时间戳结构体 */

  uint8_t eeprom_flag = AT24C128_ReadByte(COUNT_PAGE_ADDR);
  if(eeprom_flag != 0xA5) {
    _log(LOG_WARN, "EEPROM 未初始化，正在初始化...");
    AT24C128_EraseAll(); // 擦除整个EEPROM
    AT24C128_WriteByte(COUNT_PAGE_ADDR, 0xA5); // 标记已初始化
    vTaskDelay(100);
    xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_1, eSetBits, pdFALSE);// 通知上传任务初始化DTU
  }
  else {
    _log(LOG_DEBUG, "EEPROM 已初始化");
  }

  _log(LOG_DEBUG, "EEPROM 数据数量：%d", eeprom_Drive_Init());

  if(Auto_Update_TimeStamp())
  {
    xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知上传任务校准时间
  }

  print_All_Data_Pages();

  float receivedValue = 0; /* 需要保存的油量 */
  uint32_t notify_value = 0; /* 任务通知值 */

  if(eeprom_read_device_params()) {
    _log(LOG_ERROR, "EEPROM 参数错误，重新覆写！！");
    eeprom_save_device_params();
  }

  for(;;)
  {
    if(xQueueReceive(SendQueueHandle, &receivedValue, 0) == pdPASS) {  /* 接收到队列发送的流量信息 */
      oil_data.time_stamp = convert_to_timestamp();  // 获取当前RTC时间

      oil_data.oil_flow = receivedValue;
      switch(record_Oil(oil_data)) {
      case 0:
        _log(LOG_DEBUG, "流量保存成功");
        break;
      case 1:
        _log(LOG_WARN, "EEPROM 已满");
        break;
      default:
        _log(LOG_ERROR, "写入错误");
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

    xTaskNotifyWait(0x00, EVENTBIT_0 | EVENTBIT_1, &notify_value, 0);
    if(notify_value & EVENTBIT_0) {
      _log(LOG_ERROR, "正在格式化EEPROM");
      AT24C128_EraseAll(); // 擦除整个EEPROM
      _log(LOG_ERROR, "格式化完成，等待重启MCU");
      HAL_NVIC_SystemReset();
    }
    else if(notify_value & EVENTBIT_1) {
      _log(LOG_WARN, "修改参数中");
      eeprom_save_device_params();
    }
  }
}

/*oled 显示任务*/
void oled_Task(void const* argument)
{
  app_menu(); // 启动菜单任务
}

// LED控制及喂狗
void StartDefaultTask(void const* argument)
{
  HAL_IWDG_Init(&hiwdg);
  uint32_t notify_value = 0; /* 任务通知值 */
  for(;;)
  {
    xTaskNotifyWait(0x00, EVENTBIT_0, &notify_value, 200);
    if(notify_value & EVENTBIT_0) {
      LED_ON();
      vTaskDelay(25);
      LED_OFF();
      vTaskDelay(150);
      LED_ON();
      vTaskDelay(25);
      LED_OFF();
    }
    if(notify_value & EVENTBIT_1) {
      LED_ON();
    }
    HAL_IWDG_Refresh(&hiwdg); // 喂狗
    if(led_state == 2) {
      LED_Toggle();
    }
  }
}
