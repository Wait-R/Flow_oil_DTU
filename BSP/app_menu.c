#include "app_menu.h"
#include "free_menu.h"
#include "OLED.h"
#include <stdio.h>
#include "string.h"
#include "Task.h"
#include "Key.h"
#include "led.h"
#include "myRTC.h"

char* menu_main(bool action);
uint32_t fps_frame_count = 0;
uint32_t fps_last_tick = 0;
uint8_t led_state = DEFAULT_LED_STATE; // 0：led熄灭，1：常量，2：闪烁，3:信息交互时亮，4：出现错误时常量
uint8_t U1_BaudRate = DEFAULT_U1_BAUDRATE; // 波特率选择 0：4800，1：9600，2：14400，3：19200，4：38400，5：115200，6：256000；
uint8_t U3_BaudRate = DEFAULT_U3_BAUDRATE; // 波特率选择 0：4800，1：9600，2：14400，3：19200，4：38400，5：115200，6：256000；
extern __IO uint8_t DTURX_Data[1];		   		// 接收字节缓存
uint8_t UPdata_UP_flag = 1;  // 0：不上传 1：上传 （该参数不保存，仅限模拟断网调试）

void app_menu(void)
{
    menu_main(false);  // 默认不打开菜单

    while (1)
    {
        if(menu_get_focus() != NULL) // 有菜单 按键输入给到菜单
        {
            if(Key_Check(KEY0, KEY_SINGLE))
            {
                _log(LOG_DEBUG, "KEY0 DOWN");
                menu_event_inject(menu_get_focus(), MENU_EVENT_WHEEL_DOWN);
            }
            else if(Key_Check(KEY0, KEY_LONG))
            {
                _log(LOG_DEBUG, "KEY0 LONG");
                menu_event_inject(menu_get_focus(), MENU_EVENT_ENTER);
            }
            else if(Key_Check(KEY0, KEY_DOUBLE))
            {
                _log(LOG_DEBUG, "KEY0 DOUBLE");
                menu_event_inject(menu_get_focus(), MENU_EVENT_WHEEL_UP);
            }
        }
        else {
            if(Key_Check(KEY0, KEY_SINGLE))
            {
                _log(LOG_WARN, "ON MENU");
            }
            if(Key_Check(KEY0, KEY_LONG)) {
                _log(LOG_DEBUG, "打开菜单");
                menu_main(true);
            }
            if(Key_Check(KEY0, KEY_DOUBLE)) {
                _log(LOG_WARN, "ON MENU");
            }
        }

        OLED_Clear();
        menu_show(menu_get_focus());
        OLED_Update();
        vTaskDelay(20);
    }
}

// 主菜单标题
char *menu_main_title(bool action)
{
    if(action)
    {
        menu_back_focus();
        _log(LOG_WARN,"关闭菜单");
    }
    return "Main Menu";
}

// FPS显示菜单项
char *menu_fps_display(bool action)
{
    static uint8_t fps_display_buf[12];
    // 实时计算FPS
    uint32_t current_tick = HAL_GetTick();
    // 每1秒计算一次FPS
    if (current_tick - fps_last_tick >= 1000)
    {
        // 精准计算：帧率 = 总帧数 / 耗时（秒）
        float fps_current = (float)fps_frame_count / ((current_tick - fps_last_tick) / 1000.0f);
        // 格式化显示字符串（保留1位小数，对齐）
        sprintf((char*)fps_display_buf, "FPS: [%.1f]", fps_current);
        // 重置计数和时间戳
        fps_frame_count = 0;
        fps_last_tick = current_tick;
    }
    // 返回FPS显示字符串
    return (char *)fps_display_buf;
}

// 返回最开始
char* menu_Back_Top(bool action)
{
    if(action)
        menu_select_item(menu_get_focus(), 1); // 回到顶部
    return "Back to Top";
}

// 日期菜单项
char* menu_Data(bool action)
{
    static uint8_t data_display_buf[18];
    static uint8_t data_sta_flag = 0;
    uint32_t timestamp = convert_to_timestamp(); // 读取时间戳
    if(data_sta_flag == 1) {
        sprintf((char*)data_display_buf,
            "Data:%04d-%02d-%02d *",
            get_year(timestamp, 8), 
            get_month(timestamp, 8), 
            get_day(timestamp, 8));
    }
    else {
        sprintf((char*)data_display_buf,
            "Data:%04d-%02d-%02d",
            get_year(timestamp, 8), 
            get_month(timestamp, 8), 
            get_day(timestamp, 8));
    }
    
    if(action) {
        data_sta_flag = 1;
        while(1)
        {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                // 通知校准
                xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知上传任务校准时间
                break;
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            OLED_Clear();
            menu_show_focus_edit_cursor(15, 3);
            OLED_Update();
            vTaskDelay(20);
        }
        data_sta_flag = 0;
    }
    return (char*)data_display_buf;
}

// 时间菜单项
char* menu_Time(bool action)
{
    static uint8_t Time_display_buf[16] = { 0 };
    static uint8_t Time_sta_flag = 0;
    uint32_t timestamp = convert_to_timestamp(); // 读取时间戳
    
    if(Time_sta_flag == 1) {
        sprintf((char*)Time_display_buf,
            "Time:%02d:%02d:%02d *",
            get_hour(timestamp,8), 
            get_minute(timestamp,8), 
            get_second(timestamp, 8));
    }
    else {
        sprintf((char*)Time_display_buf,
            "Time:%02d:%02d:%02d",
            get_hour(timestamp,8), 
            get_minute(timestamp,8), 
            get_second(timestamp, 8));
    }
    if(action) {
        Time_sta_flag = 1;
        while(1)
        {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                // 通知校准
                xTaskNotifyFromISR(UploadDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知上传任务校准时间
                break;
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            OLED_Clear();
            menu_show_focus_edit_cursor(13, 3);
            OLED_Update();
            vTaskDelay(20);
        }
        Time_sta_flag = 0;
    }
    return (char*)Time_display_buf;
}

// 当前流量菜单项
char* menu_Current_Flow(bool action)
{
    if(action) {
        menu_back_focus();
        _log(LOG_WARN,"关闭菜单");
    }
    static uint8_t Current_Flow_buf[18];
    float current_flow = (float)Get_Traffic();
    sprintf((char*)Current_Flow_buf, "flow: %.2f L", current_flow);
    return (char*)Current_Flow_buf;
}

// 历史最大流量菜单项
char* menu_History_Max_Flow(bool action)
{
    static uint8_t History_Max_Flow_buf[21];
    static float current_Max_Flow = 0.0f; // 历史最大流量值

    float current_flow = (float)Get_Traffic();
    /* 计算历史流量最大值 */
    if(current_flow > current_Max_Flow) {
    current_Max_Flow = current_flow;
    }

    if(action) {
        _log(LOG_DEBUG, "清除本次加油流量 %.2f", current_Max_Flow);
        current_Max_Flow = 0.0f; // 重置最大流量值
    }

    sprintf((char*)History_Max_Flow_buf, "Max Flow: %.2f L", current_Max_Flow);
    return (char*)History_Max_Flow_buf;
}

// eeprom数据菜单项
char* menu_eeprom_data(bool action)
{
    static uint8_t eeprom_data_buf[18] = {0};
    uint16_t used_count = get_Data_Page_Count();  // 已存储数据个数
    sprintf((char*)eeprom_data_buf, "EEPROM: %d/%d", used_count, 2016);
    return (char*)eeprom_data_buf;
}

// 油罐车序号菜单项
char* menu_Serial_NUM(bool action)
{
    static char Serial_NUM_Buf[20] = { 0 };
    sprintf(Serial_NUM_Buf, "Serial NUM: %d", SERIAL_NUMBER);
    return Serial_NUM_Buf;
}

// 累计加油次数菜单项
char* menu_Total_Refuel_Times(bool action)
{
    static uint8_t Total_Refuel_Times_buf[25] = {0};
    sprintf((char*)Total_Refuel_Times_buf, "Refuel Times: %d", eeprom_Count);
    return (char*)Total_Refuel_Times_buf;
}

// 堆栈详情页标题
char* menu_stack_title(bool action)
{
    if(action) {
        menu_back_focus();
    }
    return "Stack View";
}

// oledTask堆栈项
char* menu_oledTask(bool action)
{
    static uint8_t oledtask_stack_buf[21];
    static uint32_t oledtask_last_tick = 0;
    
    if(HAL_GetTick() - oledtask_last_tick < 10000)
        return (char*)oledtask_stack_buf;
    
    oledtask_last_tick = HAL_GetTick();
    
    sprintf((char*)oledtask_stack_buf, "oledTask:   %03d Bit", uxTaskGetStackHighWaterMark(oledTaskHandle));
    vTaskDelay(1);
    return (char*)oledtask_stack_buf;
}

// UploadTask堆栈项
char* menu_UploadTask(bool action)
{
    static uint8_t Uploadtask_stack_buf[21];
    static uint32_t Uploadtask_last_tick = 0;
    
    if(HAL_GetTick() - Uploadtask_last_tick < 10000)
        return (char*)Uploadtask_stack_buf;
    
    Uploadtask_last_tick = HAL_GetTick();  
    sprintf((char*)Uploadtask_stack_buf, "UploadTask: %03d Bit", uxTaskGetStackHighWaterMark(UploadDataTaskHandle));
    return (char*)Uploadtask_stack_buf;
}

// ProcesTask堆栈项
char* menu_ProcesTask(bool action)
{
    static uint8_t Procestask_stack_buf[21];
    static uint32_t Procestask_last_tick = 0;
    
    if(HAL_GetTick() - Procestask_last_tick < 10000)
        return (char*)Procestask_stack_buf;
    
    Procestask_last_tick = HAL_GetTick(); 
    sprintf((char*)Procestask_stack_buf, "ProcesTask: %03d Bit", uxTaskGetStackHighWaterMark(ProcesDataTaskHandle));
    return (char*)Procestask_stack_buf;
}

// StorageTask堆栈项
char* menu_StorageTask(bool action)
{
    static uint8_t Storagetask_stack_buf[21];
    static uint32_t Storagetask_last_tick = 0;
    
    if(HAL_GetTick() - Storagetask_last_tick < 10000)
        return (char*)Storagetask_stack_buf;
    
    Storagetask_last_tick = HAL_GetTick();
    sprintf((char*)Storagetask_stack_buf, "StorageTask:%03d Bit", uxTaskGetStackHighWaterMark(StorageDataTaskHandle));
    return (char*)Storagetask_stack_buf;
}

// defaultTask堆栈项
char* menu_defaultTask(bool action)
{
    static uint8_t defaulttask_stack_buf[21];
    static uint32_t defaulttask_last_tick = 0;
    
    if(HAL_GetTick() - defaulttask_last_tick < 10000)
        return (char*)defaulttask_stack_buf;
    
    defaulttask_last_tick = HAL_GetTick();
    sprintf((char*)defaulttask_stack_buf, "defaultTask:%03d Bit", uxTaskGetStackHighWaterMark(defaultTaskHandle));
    return (char*)defaulttask_stack_buf;
}

// 系统总堆栈剩余
char* menu_RTOS_Stack(bool action)
{
    static uint8_t RTOS_Stack_buf[21] = { 0 };
    static uint32_t RTOS_last_tick = 0;

    if(HAL_GetTick() - RTOS_last_tick < 35000) {
        return (char*)RTOS_Stack_buf;
    }
    RTOS_last_tick = HAL_GetTick();

    sprintf((char *)RTOS_Stack_buf, "FreeRTOS:  %04d Bit", xPortGetMinimumEverFreeHeapSize());
    return (char *)RTOS_Stack_buf;
}

// 设置页标题
char* menu_Setting_title(bool action)
{
    if(action) {
        menu_back_focus();
    }
    return "Setting";
}

// LED控制菜单项
char* menu_Set_led(bool action)
{
    static uint8_t led_buf[14] = { 0 };

    switch (led_state)
    {
    case 0:
        // LED 熄灭
        LED_OFF();
        sprintf((char*)led_buf, "LED: [OFF]");
        break;
    case 1:
        // LED 常量
        LED_ON();
        sprintf((char*)led_buf, "LED: [ON]");
        break;
    case 2:
        // LED 闪烁
        sprintf((char*)led_buf, "LED: [Toggle]");
        break;
    case 3:
        // LED 信息交互时闪烁
        sprintf((char*)led_buf, "LED: [IEC]");
        break;
    case 4:
        // 出现错误时LED常亮
        sprintf((char*)led_buf, "LED: [W/ER]");
        break;
    default:
        _log(LOG_ERROR, "");
        Error_Handler();
        break;
    }

    if(action) {
        while(1) {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                led_state++;
                if(led_state > 4) {
                    led_state = 0;
                }
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            char str[9];
            switch (led_state)
            {
            case 0:
                // LED 熄灭
                LED_OFF();
                sprintf((char*)str, "[OFF]");
                _log(LOG_WARN, "LED OFF");
                break;
            case 1:
                // LED 常量
                LED_ON();
                sprintf((char*)str, "[ON]");
                _log(LOG_WARN, "LED ON");
                break;
            case 2:
                // LED 闪烁
                sprintf((char*)str, "[Toggle]");
                _log(LOG_WARN, "LED Toggle");
                break;
            case 3:
                // LED 信息交互时闪烁
                sprintf((char*)str, "[IEC]");
                _log(LOG_WARN, "LED TEC 仅信息交互时闪烁");
                break;
            case 4:
                // 出现错误时LED常亮
                sprintf((char*)str, "[W/ER]");
                _log(LOG_WARN, "LED ER 出现错误时常量");
                break;
            default:
                break;
            }
            OLED_Clear();
            menu_show_focus_edit_cursor(4, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        xTaskNotifyFromISR(StorageDataTaskHandle, EVENTBIT_1, eSetBits, pdFALSE);// 通知存储任务保存参数
    }

    return (char*)led_buf;
}

// 日志级别控制菜单项
char* menu_Set_LOG(bool action)
{
    static uint8_t LOG_LEVEL_TEXT_FLAG = LOG_LEVEL_INIT;
    static uint8_t LOG_LE_Buf[20] = { 0 };

    switch (LOG_LEVEL_TEXT_FLAG)
    {
    case 0:
        // 关闭DEBUG级别输出
        sprintf((char*)LOG_LE_Buf, "LOG LEVEL: [OFF]");
        break;
    case 1:
        // 简略DEBUG、WARN信息
        sprintf((char*)LOG_LE_Buf, "LOG LEVEL: [D/W]");
        break;
    case 2:
        // 简略DEBUG信息
        sprintf((char*)LOG_LE_Buf, "LOG LEVEL: [D]");
        break;
    case 3:
        // 不省略
        sprintf((char*)LOG_LE_Buf, "LOG LEVEL: [ON]");
        break;
    default:
        _log(LOG_ERROR, "");
        break;
    }

    if(action) {
        while(1) {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                LOG_LEVEL_TEXT_FLAG++;
                if(LOG_LEVEL_TEXT_FLAG > 3) {
                    LOG_LEVEL_TEXT_FLAG = 0;
                }
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            char str[6] = { 0 };
            switch (LOG_LEVEL_TEXT_FLAG)
            {
            case 0:
                // 关闭DEBUG级别输出
                sprintf((char*)str, "[OFF]");
                break;
            case 1:
                // 简略DEBUG、WARN信息
                sprintf((char*)str, "[D/W]");
                break;
            case 2:
                // 简略DEBUG信息
                sprintf((char*)str, "[D]");
                break;
            case 3:
                // 不省略
                sprintf((char*)str, "[ON]");
                break;
            default:
                _log(LOG_ERROR, "");
                LOG_LEVEL_TEXT_FLAG = LOG_LEVEL_INIT;
                break;
            }
            OLED_Clear();
            menu_show_focus_edit_cursor(10, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        _log(LOG_WARN, "日志等级调整中");
        LOG_Detailed_inf = LOG_LEVEL_TEXT_FLAG;
        xTaskNotifyFromISR(StorageDataTaskHandle, EVENTBIT_1, eSetBits, pdFALSE);// 通知存储任务保存参数
    }

    return (char*)LOG_LE_Buf;
}

// 调试串口波特率选择
char* menu_Set_U1_BaudRate(bool action)
{
    static uint8_t U1_BT[14] = { 0 };

    switch (U1_BaudRate)
    {
    case 0:
        // 4800 波特率
        sprintf((char*)U1_BT, "U1B: [4800]");
        break;
    case 1:
        // 9600 波特率
        sprintf((char*)U1_BT, "U1B: [9600]");
        break;
    case 2:
        // 14400 波特率
        sprintf((char*)U1_BT, "U1B: [14400]");
        break;
    case 3:
        // 19200 波特率
        sprintf((char*)U1_BT, "U1B: [19200]");
        break;
    case 4:
        // 38400 波特率
        sprintf((char*)U1_BT, "U1B: [38400]");
        break;
    case 5:
        // 115200 波特率
        sprintf((char*)U1_BT, "U1B: [115200]");
        break;
    case 6:
        // 256000 波特率
        sprintf((char*)U1_BT, "U1B: [256000]");
        break;
    default:
        sprintf((char*)U1_BT, "U1B: [OvO]");
        _log(LOG_ERROR, "波特率不符合预期输入");
        U1_BaudRate = 5;
        break;
    }

    if(action) {
        int BaudRate = 115200;
        while(1)
        {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                U1_BaudRate++;
                if(U1_BaudRate > 6) {
                    U1_BaudRate = 0;
                }
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            char str[9] = { 0 };
            switch (U1_BaudRate)
            {
            case 0:
                // 4800 波特率
                BaudRate = 4800;
                sprintf((char*)str, "[4800]");
                break;
            case 1:
                // 9600 波特率
                BaudRate = 9600;
                sprintf((char*)str, "[9600]");
                break;
            case 2:
                // 14400 波特率
                BaudRate = 14400;
                sprintf((char*)str, "[14400]");
                break;
            case 3:
                // 19200 波特率
                BaudRate = 19200;
                sprintf((char*)str, "[19200]");
                break;
            case 4:
                // 38400 波特率
                BaudRate = 38400;
                sprintf((char*)str, "[38400]");
                break;
            case 5:
                // 115200 波特率
                BaudRate = 115200;
                sprintf((char*)str, "[115200]");
                break;
            case 6:
                // 256000 波特率
                BaudRate = 256000;
                sprintf((char*)str, "[256000]");
                break;
            default:
                sprintf((char*)str, "[OvO]");
                _log(LOG_ERROR, "");
                U1_BaudRate = 5;
                BaudRate = 115200;
                break;
            }

            OLED_Clear();
            menu_show_focus_edit_cursor(4, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        // 在此处修改波特率
        Runing_UARTSET_B(&huart1, BaudRate, NULL);
        _log(LOG_WARN, "已发送修改申请");
        xTaskNotifyFromISR(StorageDataTaskHandle, EVENTBIT_1, eSetBits, pdFALSE);// 通知存储任务保存参数
    }
    return (char *)U1_BT;
}

// DTU通信串口波特率选择
char* menu_Set_U3_BaudRate(bool action)
{
    static uint8_t U3_BT[14] = { 0 };

    switch (U3_BaudRate)
    {
    case 0:
        // 4800 波特率
        sprintf((char*)U3_BT, "U1B: [4800]");
        break;
    case 1:
        // 9600 波特率
        sprintf((char*)U3_BT, "U3B: [9600]");
        break;
    case 2:
        // 14400 波特率
        sprintf((char*)U3_BT, "U3B: [14400]");
        break;
    case 3:
        // 19200 波特率
        sprintf((char*)U3_BT, "U3B: [19200]");
        break;
    case 4:
        // 38400 波特率
        sprintf((char*)U3_BT, "U3B: [38400]");
        break;
    case 5:
        // 115200 波特率
        sprintf((char*)U3_BT, "U3B: [115200]");
        break;
    case 6:
        // 256000 波特率
        sprintf((char*)U3_BT, "U3B: [256000]");
        break;
    default:
        sprintf((char*)U3_BT, "U3B: [OvO]");
        _log(LOG_ERROR, "波特率不符合预期输入");
        U3_BaudRate = 5;
        break;
    }

    if(action) {
        int BaudRate = 115200;
        while(1)
        {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                U3_BaudRate++;
                if(U3_BaudRate > 6) {
                    U3_BaudRate = 0;
                }
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            char str[9] = { 0 };
            switch (U3_BaudRate)
            {
            case 0:
                // 4800 波特率
                BaudRate = 4800;
                sprintf((char*)str, "[4800]");
                break;
            case 1:
                // 9600 波特率
                BaudRate = 9600;
                sprintf((char*)str, "[9600]");
                break;
            case 2:
                // 14400 波特率
                BaudRate = 14400;
                sprintf((char*)str, "[14400]");
                break;
            case 3:
                // 19200 波特率
                BaudRate = 19200;
                sprintf((char*)str, "[19200]");
                break;
            case 4:
                // 38400 波特率
                BaudRate = 38400;
                sprintf((char*)str, "[38400]");
                break;
            case 5:
                // 115200 波特率
                BaudRate = 115200;
                sprintf((char*)str, "[115200]");
                break;
            case 6:
                // 256000 波特率
                BaudRate = 256000;
                sprintf((char*)str, "[256000]");
                break;
            default:
                sprintf((char*)str, "[OvO]");
                _log(LOG_ERROR, "");
                BaudRate = 115200;
                U3_BaudRate = 5;
                break;
            }

            OLED_Clear();
            menu_show_focus_edit_cursor(4, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        // 在此处修改波特率
        Runing_UARTSET_B(&huart3, BaudRate, (uint8_t*)DTURX_Data);
        xTaskNotifyFromISR(StorageDataTaskHandle, EVENTBIT_1, eSetBits, pdFALSE);// 通知存储任务保存参数
    }
    return (char *)U3_BT;
}

// 上传使能
char* Menu_UPData_EN(bool action)
{
    static uint8_t EN_Data_buf[14] = { 0 };
    if(UPdata_UP_flag == 0) {
        // 不上传
        sprintf((char*)EN_Data_buf, "UPdata: [DIS]");
    }
    else {
        // 上传
        sprintf((char*)EN_Data_buf, "UPdata: [EN]");
    }

    if(action) {
        while (1)
        {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                UPdata_UP_flag++;
                if(UPdata_UP_flag > 1) UPdata_UP_flag = 0;
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;

            char str[7] = { 0 };
            if(UPdata_UP_flag == 0) {
                // 不上传
                sprintf((char*)str, "[DIS]");
            }
            else {
                // 上传
                sprintf((char*)str, "[EN]");
            }
            OLED_Clear();
            menu_show_focus_edit_cursor(7, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        if(UPdata_UP_flag == 0)
            _log(LOG_WARN, "上传已关闭-此选项不保存");
    }
    return (char*)EN_Data_buf;
}

// 生成随机数
float myRand(void)
{
    srand((unsigned int)convert_to_timestamp());

    float random_val = 10.0f + (rand() / (float)RAND_MAX) * 90.0f;

    return random_val;
}

// 随机上传一个流量数据
char* menu_Rand_flow(bool action)
{
    if(action) {
        float text = myRand();
        xQueueSend(SendQueueHandle, &text, 0); // 发送测试
        _log(LOG_WARN, "上传了一个随机数:%.2f", text);
    }
    return "UP Random *";
}

// 格式化EEPROM
char* menu_Format(bool action)
{
    static int8_t Format_Count = 10;
    static uint8_t Format_buf[18] = { 0 };

    sprintf((char*)Format_buf, "Format EEPROM: %d", Format_Count);

    if(action) {
        while(1) {
            if(Key_Check(KEY0, KEY_SINGLE)) {
                Format_Count--;
                if(Format_Count < 0) Format_Count = 0;
            }
            else if(Key_Check(KEY0, KEY_LONG)) break;


            char str[3] = { 0 };
            sprintf(str, "%d", Format_Count);

            OLED_Clear();
            menu_show_focus_edit_cursor(14, strlen(str)+2);
            OLED_Update();
            vTaskDelay(20);
        }
        if(Format_Count == 0) {
            xTaskNotifyFromISR(StorageDataTaskHandle, EVENTBIT_0, eSetBits, pdFALSE);// 通知存储任务格式化EEPROM
        }
        Format_Count = 10;
    }
    return (char*)Format_buf;
}

// 设置参数菜单
char* menu_set_sys(bool action)
{
    if(action) {
        static MenuItemTypeFunc Setting_fn_iyems[] = {
            menu_Setting_title,
            menu_Set_led,
            menu_Set_LOG,
            menu_Set_U1_BaudRate,
            menu_Set_U3_BaudRate,
            Menu_UPData_EN,
            menu_Rand_flow,
            menu_Format,
            menu_Back_Top,
            NULL              // 结束标记
        };
        static Menu Setting_menu;
        menu_init(&Setting_menu, Setting_fn_iyems, menu_item_handler_func);
        menu_set_focus(&Setting_menu);
    }
    return "Setting...";
}

// 查看堆栈情况菜单
char* menu_stack_view(bool action)
{
    if(action) {
        static MenuItemTypeFunc Stack_fn_iyems[] = {
            menu_stack_title,
            menu_oledTask,
            menu_UploadTask,
            menu_ProcesTask,
            menu_StorageTask,
            menu_defaultTask,
            menu_RTOS_Stack,
            menu_Back_Top,
            NULL              // 结束标记
        };
        static Menu Stack_menu;
        menu_init(&Stack_menu, Stack_fn_iyems, menu_item_handler_func);
        menu_set_focus(&Stack_menu);
    }
    return "View Stack";
}

// 主菜单初始化
char *menu_main(bool action)
{
    if (action)
    {
        OLED_Init();
        OLED_Clear();
        OLED_Update();

        static MenuItemTypeFunc fn_items[] = {
            menu_main_title,  // 主菜单标题
            menu_Current_Flow,  // 当前流量
            menu_History_Max_Flow,  // 历史最大流量
            menu_eeprom_data,   // EEPROM当前数据个数
            menu_Total_Refuel_Times,  // 累计发送了多少个数据
            menu_Serial_NUM,     // 车辆编号
            menu_fps_display,    // 屏幕刷新帧率
            menu_Data,           // 日期信息
            menu_Time,           // 时间信息
            menu_stack_view,     // 堆栈信息
            menu_set_sys,        // 系统参数设置
            menu_Back_Top,
            NULL              // 结束标记
        };
        static Menu menu;
        menu_init(&menu, fn_items, menu_item_handler_func);
        menu_set_focus(&menu);
    }
    return "Main Menu";
}

