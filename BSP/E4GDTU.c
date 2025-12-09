#include "E4GDTU.h"
#include "Task.h"
#include "string.h"
#include "stdio.h"
#include "myRTC.h"

__IO uint8_t DTURX_buf[128] = { 0 };       		// 接收命令缓存数组
__IO uint8_t DTURX_Data[1] = { 0 };		   		// 接收字节缓存
__IO uint16_t DTURX_cnt = 0, DTURX_cntPre = 0;  // 接收累计计数、接收上次计数
char mqtt_payload[128] = { 0 };					// MQTT发布载荷缓存
// char mqtt_Contrast[128] = { 0 };				// MQTT对比载荷缓存

/**
 * @brief 清空缓存
 * @param NULL
 * @return NULL
 */
static void DTU_Clear(void)
{
	memset((void *)DTURX_buf, 0, sizeof(DTURX_buf));
	DTURX_cnt = 0;
}

/**
 * @brief 等待接收完成
 * @param NULL
 * @return REV_OK-接收完成		REV_WAIT-接收超时未完成
 */
static uint8_t DTU_WaitRecive(void)
{
	if(DTURX_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
		return REV_WAIT;	
	if(DTURX_cnt == DTURX_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
	{
		DTURX_cnt = 0;							//清0接收计数	
		return REV_OK;							//返回接收完成标志
	}	
	DTURX_cntPre = DTURX_cnt;					//置为相同
	return REV_WAIT;							//返回接收未完成标志
}

/**
 * @brief 发送 AT 命令并等待响应（自动清空缓冲区）
 * @param cmd 要发送的 AT 命令（如 "AT+CIPSNTPTIME?\r\n"）
 * @param res 要等待的响应（如 "OK"）
 * @return 0 成功，1 失败
 */
static uint8_t DTU_SendCmd(const char *cmd, char *res)
{
	DTU_Clear();
	uint16_t timeOut = 0;
	HAL_UART_Transmit(&huart3, (unsigned char *)cmd, strlen((const char *)cmd), 10);

	while(timeOut < DF_RECIVE_TIMEOUT)
	{
		timeOut += 20;												//每次延时20ms
		if(DTU_WaitRecive() == REV_OK)							    //如果收到数据
		{
			if(strstr((const char *)DTURX_buf, res) != NULL)		//如果检索到关键词
			{
				DTU_Clear();									    //清空缓存
				return OK;
			}
		}
		vTaskDelay(20);
	}
	return NO;
}

/**
 * @brief 发送 AT 命令并等待响应（不自动清空缓冲区）
 * @param cmd 要发送的 AT 命令（如 "AT+CIPSNTPTIME?\r\n"）
 * @param res 要等待的响应（如 "OK"）
 * @return 0 成功，1 失败
 */
static uint8_t DTU_SendCommandRaw(const char* cmd, char *res)
{
	DTU_Clear();
	uint16_t timeOut = 0;
	HAL_UART_Transmit(&huart3, (unsigned char *)cmd, strlen((const char *)cmd), 10);

	while(timeOut < DF_RECIVE_TIMEOUT)
	{
		timeOut += 20;												//每次延时20ms
		if(DTU_WaitRecive() == REV_OK)							//如果收到数据
		{
			if(strstr((const char *)DTURX_buf, res) != NULL)		//如果检索到关键词
			{
				return OK;
			}
		}
		vTaskDelay(20);
	}
	return NO;
}

/**
 * @brief 重新启动DTU模块
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_RESET(void)
{
	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET); // 先拉低强制关机
	vTaskDelay(300);
	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET); // 恢复
	vTaskDelay(2000);
	DTU_Clear();
	return OK;
}

/**
 * @brief 关闭DTU模块
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_OFF(void)
{
	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET); // 先拉低 关机
	return OK;
}

/**
 * @brief 启动DTU模块
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_ON(void)
{
	HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET);   // 恢复
	vTaskDelay(3000);
	DTU_Clear();
	return OK;
}

/**
 * @brief 初始化DTU模块
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_Init(void)
{
	uint8_t time_out = 0;
	DTU_RESET();   // 复位DTU
	HAL_UART_Receive_IT(&huart3, (uint8_t*)DTURX_Data, 1);
	vTaskDelay(1000);

	_log(LOG_DEBUG, "1. 进入指令模式");    	// 进入指令模式
	while(DTU_SendCmd("+++", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "2. 测试AT指令");    	// 测试
	while(DTU_SendCmd("AT\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "3. 设置不缓存数据");    	// 设置不缓存数据
	while(DTU_SendCmd("AT+CACHEEN=OFF\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "4. AT+WKMOD1=MQTT...");    	// 工作模式设为MQTT
	while(DTU_SendCmd("AT+WKMOD1=MQTT\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "5. AT+MQTTSV1=8.137.124.220,1883...");    	// 设置域名或端口
	while(DTU_SendCmd("AT+MQTTSV1=8.137.124.220,1883\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "6. AT+MQTTCONN=esp8266Client,test,12345678,60,1...");    	// 设置MQTT连接参数
	while(DTU_SendCmd("AT+MQTTCONN=esp8266Client,test,12345678,120,1\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "7. AT+MQTTSUB1=/fuel/data,0...");    	// 设置订阅主题
	while(DTU_SendCmd("AT+MQTTSUB1=/fuel/data,0\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "8. AT+MQTTPUB1=/fuel/data,0,0...");    	// 设置发布主题
	while(DTU_SendCmd("AT+MQTTPUB1=/fuel/data,0,0\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

	_log(LOG_DEBUG, "9. AT+S...");    	// 保存配置
	while(DTU_SendCmd("AT+S\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(1000);

	_log(LOG_DEBUG, "10.DTU_Init OK");
	return OK;
}

/**
 * @brief 上传流量+时间数据 -- 内部调用	
 * @param oil_data 【流量+时间】数据结构体
 * @return 0 成功，1 失败
 */
static uint8_t DTU_Send_DataFloat_Internal(OilDataWithTime* oil_data)
{
	uint8_t valen;
	_log(LOG_DEBUG,"已接收上传数据: %.3f L, 时间戳：%ld S", oil_data->oil_flow, oil_data->time_stamp);
	
	sprintf(mqtt_payload, "{\"timestamp\":%ld000,\"fuel_total\":%f,\"number\":%d}\r\n", oil_data->time_stamp, oil_data->oil_flow, SERIAL_NUMBER);
	_log(LOG_DEBUG,"发送载荷%s", mqtt_payload);
	if(DTU_SendCmd(mqtt_payload, mqtt_payload) == OK) {
		_log(LOG_DEBUG, "数据上传成功！！！");
		if(xQueueSend(DeleteQueueHandle,&valen,0) != pdPASS) {
			_log(LOG_ERROR, "错误：无法发送到请求删除队列\r\n");
			return NO;
		}
	}
	vTaskDelay(50);
	memset(mqtt_payload, 0, sizeof(mqtt_payload));  // 清空载荷缓存
	// memset(mqtt_Contrast, 0, sizeof(mqtt_Contrast));  // 清空对比载荷缓存

	return OK;
}

/**
 * @brief 上传流量+时间数据
 * @param oil_data 【流量+时间】数据结构体
 * @return 0 成功，1 失败
 */
uint8_t DTU_Send_DataFloat(OilDataWithTime* oil_data)
{

	// if(DTU_Connect_MQTT() == NO) return NO;   		// 建立MQTT连接

	if(DTU_Send_DataFloat_Internal(oil_data) == NO) return NO;  // 发送数据

	// if(DTU_Disconnect_MQTT() == NO) return NO;  // 断开MQTT连接
	
	return OK;
}

/**
 * @brief 判断DTU是否可以连接上MQTT服务器
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_IsConnected_MQTT(void)
{
	uint8_t time_out = 0;
	uint8_t sta = OK;

//	_log(LOG_DEBUG, "1. 进入指令模式");    	// 进入指令模式
	while(DTU_SendCmd("+++", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);

//	_log(LOG_DEBUG, "2. 查询连接状态");    	// 查询连接状态
	while(DTU_SendCmd("AT+SOCKLK1?\r\n", "Connected")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			sta = NO;
			break;
		}
	}
	vTaskDelay(100);

//	_log(LOG_DEBUG, "3. 退出指令模式");    	// 退出指令模式
	while(DTU_SendCmd("AT+ENTM\r\n", "OK")) {
		vTaskDelay(100);
		time_out += 1;
		if(time_out > DF_TIMEOUT) {
			time_out = 0;
			return NO;
		}
	}
	vTaskDelay(100);
	return sta;
}

/**
 * @brief 获取当前网络时间
 * @param NULL
 * @return 0 成功，1 失败
 */
uint8_t DTU_Get_NTP_Time(void)
{
	uint8_t time_out = 0;
	uint8_t sta = OK;
	tm_t ntp_tm = { 0 };
    char *time_start = NULL;
    int year, month, day, hour, minute, second;

    // 1. 进入指令模式
    while (DTU_SendCmd("+++", "OK")) {
        vTaskDelay(100);
        time_out++;
        if (time_out > DF_TIMEOUT) {
            time_out = 0;
            return NO; // 进入指令模式失败
        }
    }
    vTaskDelay(200); 

    // 2. 发送AT+CCLK?查询NTP时间
    time_out = 0;
    while (DTU_SendCommandRaw("AT+CCLK?\r\n", "OK")) {
        vTaskDelay(100);
        time_out++;
        if (time_out > DF_TIMEOUT) {
            time_out = 0;
			sta = NO;  // 查询时间失败
			break;
		}
    }
    vTaskDelay(100);

    // 3. 解析DTURX_buf中的时间数据（响应格式：+CCLK:2025/11/11,17:55:23）
    // 步骤1：找到"+CCLK:"后的时间起始位置
    time_start = strstr((char *)DTURX_buf, "+CCLK:");
    if (time_start == NULL) {
        sta = NO;
    }
    time_start += 6;  // 跳过"+CCLK:"（长度6），指向时间字符串开头

    // 步骤2：按格式解析时间（YYYY/MM/DD,HH:MM:SS）
    if (sscanf(time_start, "%d/%d/%d,%d:%d:%d", 
        &year, &month, &day, &hour, &minute, &second) != 6) {
        sta = NO;
	}

	if(year < 2025) sta = NO; // 年份异常

    // 步骤3：填充tm结构体
    ntp_tm.tm_year = year - 1900;  // tm_year为年份-1900（如2025→125）
    ntp_tm.tm_mon = month - 1;     // tm_mon为0-11（如11月→10）
    ntp_tm.tm_mday = day;          // 日期1-31
    ntp_tm.tm_hour = hour;         // 小时0-23
    ntp_tm.tm_min = minute;        // 分钟0-59
    ntp_tm.tm_sec = second;        // 秒0-59
    ntp_tm.tm_isdst = 0;           // 禁用夏令时

	// 步骤4：调用RTC_SET_TIME设置时间到硬件RTC
	if(year >= 2041 || year < 2025) {
		sta = NO; // 年份异常
	}
	else {
		RTC_SET_TIME(&ntp_tm);
	}
	

    // 4. 退出指令模式，返回透传模式
    time_out = 0;
    while (DTU_SendCmd("AT+ENTM\r\n", "OK")) {
        vTaskDelay(100);
        time_out++;
        if (time_out > DF_TIMEOUT) {
            time_out = 0;
            return NO;  // 退出指令模式失败（但RTC已设置，可根据需求调整返回值）
        }
    }
    vTaskDelay(50);

    return sta;  // 全部流程成功
}

/**
 * @brief       Rx传输回调函数
 * @param       huart: UART句柄类型指针
 * @retval      无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART3)             /* 如果是串口3 */
	{
		if(DTURX_cnt >= sizeof(DTURX_buf))	DTURX_cnt = 0; //防止串口被刷爆
		DTURX_buf[DTURX_cnt++] = DTURX_Data[0];
		
		HAL_UART_Receive_IT(&huart3, (uint8_t *)DTURX_Data, 1);
	}
}
