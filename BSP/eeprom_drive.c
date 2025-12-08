#include "eeprom_drive.h"
#include "at24c64.h"  // 使用AT24C128 EEPROM驱动
#include "i2c.h"
#include "delay.h"
#include "stdio.h"
#include "Task.h"
#include "string.h"
#include "Capture_Traffic.h"
#include <math.h>
#include "myRTC.h"
#include "app_menu.h"

#define OIL_DATA_LEN  sizeof(OilDataWithTime)  // 结构体字节数（8字节）
#define DATA_PER_PAGE (AT24C128_PAGE_SIZE / OIL_DATA_LEN)  // 每页存储的数据个数（64/8=8）
#define DATA_START_PAGE 4                      // 数据存储起始页（第4页）
#define DATA_TOTAL_PAGES (256 - DATA_START_PAGE)  // 可用数据总页数（256-4=252页）
#define MAX_DATA_COUNT (DATA_TOTAL_PAGES * DATA_PER_PAGE)  // 最大可存储数据总数（252×8=2016）

static uint8_t data_status[(MAX_DATA_COUNT + 7) / 8];  // 数据状态位图（1位表示1条数据是否占用）
static uint16_t eeprom_data_count = 0;  // 当前存储的数据总数
uint32_t eeprom_Count = 0; // 存储加油次数

/************************************************
EEPROM 数据存储布局：
- 第0页：存储初始化标志
- 第1页：存储时间戳
- 第2页：存储加油次数
- 第3页：保留
- 第4页起：存储 OilDataWithTime 结构体数据
************************************************/

/**
 * @brief 将数据索引转换为EEPROM物理地址
 * @param index 数据索引（0~MAX_DATA_COUNT-1）
 * @return 对应的EEPROM地址
 */
static uint16_t data_index_to_addr(uint16_t index)
{
    uint16_t page = DATA_START_PAGE + (index / DATA_PER_PAGE);  // 计算数据所在页
    uint16_t offset = (index % DATA_PER_PAGE) * OIL_DATA_LEN;   // 计算页内偏移
    return PAGE_ADDR(page) + offset;
}

/**
 * @brief 查找第一个空闲的数据索引
 * @return 空闲索引（0~MAX_DATA_COUNT-1），-1表示无空闲位置
 */
static int16_t find_first_free_index(void)
{
    for (uint16_t i = 0; i < MAX_DATA_COUNT; i++) {
        uint8_t byte = i / 8;
        uint8_t bit = i % 8;
        if (!(data_status[byte] & (1 << bit))) {
            return i;  // 找到空闲位置
        }
    }
    return -1;  // 存储已满
}

/**
 * @brief 设置数据索引的占用状态
 * @param index 数据索引（0~MAX_DATA_COUNT-1）
 * @param is_used 1:标记为占用，0:标记为空闲
 */
static void set_data_status(uint16_t index, uint8_t is_used)
{
    if (index >= MAX_DATA_COUNT) return;
    uint8_t byte = index / 8;
    uint8_t bit = index % 8;
    if (is_used) {
        data_status[byte] |= (1 << bit);  // 标记为占用
    } else {
        data_status[byte] &= ~(1 << bit); // 标记为空闲
    }
}

/**
 * @brief 检查数据索引是否被占用
 * @param index 数据索引（0~MAX_DATA_COUNT-1）
 * @return 1:已占用，0:空闲
 */
static uint8_t is_data_used(uint16_t index)
{
    if (index >= MAX_DATA_COUNT) return 0;
    uint8_t byte = index / 8;
    uint8_t bit = index % 8;
    return (data_status[byte] & (1 << bit)) ? 1 : 0;
}

/**
 * @brief 向EEPROM写入【油量+时间戳】结构体
 * @param u16Addr 写入的起始地址
 * @param pData 要写入的结构体指针
 */
void x24Cxx_Write_float(uint16_t u16Addr, OilDataWithTime *pData)
{
    uint8_t buf[OIL_DATA_LEN];
    uint8_t *p = (uint8_t *)pData;
    // 将结构体数据转换为字节数组
    for (uint8_t i = 0; i < OIL_DATA_LEN; i++) {
        buf[i] = *(p + i);
    }
    // 写入EEPROM
    AT24C128_WritePage(u16Addr, buf, OIL_DATA_LEN);
}

/**
 * @brief 从EEPROM读取【油量+时间戳】结构体
 * @param u16Addr 读取的起始地址
 * @param pData 存储读取结果的结构体指针
 */
void x24Cxx_Read_float(uint16_t u16Addr, OilDataWithTime *pData)
{
    uint8_t buf[OIL_DATA_LEN];
    // 从EEPROM读取字节数组
    AT24C128_ReadBuffer(u16Addr, buf, OIL_DATA_LEN);
    // 将字节数组转换为结构体
    *pData = *(OilDataWithTime *)buf;
}

/**
 * @brief 初始化EEPROM驱动
 * @return 当前已存储的数据个数
 */
uint16_t eeprom_Drive_Init(void)
{
    iic_init();  // 初始化I2C总线
    delay_ms(5);
    eeprom_read_counter(&eeprom_Count); // 读取加油次数
    delay_ms(5);
    eeprom_data_count = 0;
    memset(data_status, 0, sizeof(data_status));  // 初始化状态位图

    uint8_t buf[OIL_DATA_LEN];
    // 扫描所有可能的数据位置，建立状态位图
    for (uint16_t i = 0; i < MAX_DATA_COUNT; i++) {
        uint16_t addr = data_index_to_addr(i);
        AT24C128_ReadBuffer(addr, buf, OIL_DATA_LEN);
        
        // 检查是否为有效数据（非0xFF）
        uint8_t is_valid = 0;
        for (uint8_t j = 0; j < OIL_DATA_LEN; j++) {
            if (buf[j] != 0xFF) {
                is_valid = 1;
                break;
            }
        }
        
        if (is_valid) {
            set_data_status(i, 1);  // 标记为占用
            eeprom_data_count++;
        }
    }
    return eeprom_data_count;
}

/**
 * @brief 记录【油量+时间戳】数据到EEPROM
 * @param oil_data 要存储的【油量+时间戳】结构体
 * @return 0:成功, 1:EEPROM已满, 2:写入失败
 */
uint8_t record_Oil(OilDataWithTime oil_data)
{
    eeprom_increment_counter(); // 计数器自增
    delay_ms(5);
    // 查找空闲位置
    int16_t free_index = find_first_free_index();
    if (free_index == -1) {
        return 1;  // EEPROM已满
    }
    
    // 计算存储地址并写入数据
    uint16_t storage_addr = data_index_to_addr(free_index);
    x24Cxx_Write_float(storage_addr, &oil_data);
    delay_ms(5);  // 等待写入完成
    
    // 验证写入数据
    OilDataWithTime read_back;
    x24Cxx_Read_float(storage_addr, &read_back);
    if ((read_back.oil_flow != oil_data.oil_flow) || (read_back.time_stamp != oil_data.time_stamp)) {
        return 2;  // 写入失败
    }
    
    // 更新状态和计数
    set_data_status(free_index, 1);
    eeprom_data_count++;
    
    return 0;  // 成功
}

/**
 * @brief 读取最后一条【油量+时间戳】数据并擦除
 * @note 从消息队列接收删除请求，读取最后一条数据后发送到上传队列，
 *       收到上传完成信号后擦除该数据
 */
void read_Oil_Data(void)
{
    uint8_t received_request;
    if(xQueueReceive(RequestQueueHandle, &received_request, 0) == pdPASS) { /* 获取二值信号量 */  
			_log(LOG_DEBUG,"已接收请求");

        // 查找最后一条有数据的索引
        int16_t last_index = -1;
        for (int16_t i = MAX_DATA_COUNT - 1; i >= 0; i--) {
            if (is_data_used(i)) {
                last_index = i;
                break;
            }
        }
        
        if (last_index == -1) {
					_log(LOG_WARN,"无数据可读取");
            return;
        }
        
        // 读取数据
        OilDataWithTime read_data;
        uint16_t storage_addr = data_index_to_addr(last_index);
        x24Cxx_Read_float(storage_addr, &read_data);

        // 发送到上传队列
        if (xQueueSend(UploadQueueHandle, &read_data, 0) != pdPASS) {
					_log(LOG_ERROR,"错误：数据无法发送到上传队列");
            return;
        }
        else {
					_log(LOG_DEBUG,"数据:%.3f 已发送到上传队列", read_data.oil_flow);
            // 等待上传完成信号
            if (xQueueReceive(DeleteQueueHandle, &received_request, 10000) == pdPASS) {
                // 擦除该数据（填充0xFF）
                uint8_t blank[OIL_DATA_LEN];
                memset(blank, 0xFF, OIL_DATA_LEN);
                AT24C128_WritePage(storage_addr, blank, OIL_DATA_LEN);
                delay_ms(5);  // 等待写入完成
                
                // 更新状态和计数
                set_data_status(last_index, 0);
                eeprom_data_count--;
                
							_log(LOG_DEBUG,"成功上传,本地删除,剩余：%d", get_Data_Page_Count());
            }
            else {
							_log(LOG_WARN,"上传失败");
            }
        }
    }
}

/**
 * @brief 打印所有存储的【油量+时间戳】数据
 * @note 调试用，输出所有数据的索引、地址、油量和时间戳
 */
void print_All_Data_Pages(void)
{
	_log(LOG_DEBUG, "=== EEPROM数据信息 ===");
	_log(LOG_DEBUG, "总数据量: %d/%d", eeprom_data_count, MAX_DATA_COUNT);
    
    uint16_t count = 0;
    // 遍历所有数据位置，打印有效数据
    for (uint16_t i = 0; i < MAX_DATA_COUNT; i++) {
        if (is_data_used(i)) {
            OilDataWithTime data;
            x24Cxx_Read_float(data_index_to_addr(i), &data);
					_log(LOG_DEBUG, "索引%d: 地址0x%04X, 数据: %.2f | %lu",
														i, data_index_to_addr(i), data.oil_flow, data.time_stamp);
            count++;
            if (count >= eeprom_data_count) break;  // 已打印所有数据，提前退出
        }
    }
    if (count == 0) {
			_log(LOG_DEBUG, "没有找到数据");
    }
}

/**
 * @brief 获取当前存储的数据总数
 * @return 已存储的数据个数
 */
uint16_t get_Data_Page_Count(void)
{
    return eeprom_data_count;
}

/**
 * @brief 擦除整个EEPROM（填充0xFF）
 * @note 将所有存储单元写入0xFF，恢复为初始状态
 */
void AT24C128_EraseAll(void)
{
    uint8_t blank_page[AT24C128_PAGE_SIZE];
    // 填充擦除数据（0xFF）
    memset(blank_page, 0xFF, AT24C128_PAGE_SIZE);
    
    // 按页擦除整个EEPROM
    for (uint16_t page_addr = 0; page_addr <= AT24C128_MAX_ADDR; page_addr += AT24C128_PAGE_SIZE) {
        AT24C128_WritePage(page_addr, blank_page, AT24C128_PAGE_SIZE);
        delay_ms(5);  // 等待每页写入完成
    }
}

#define COUNTER_ADDR (COEFFICIENT_PAGE_ADDR + 8) // 假设 double 占 8 字节，整数从第 8 字节开始
#define COUNTER_SIZE sizeof(uint32_t)            // 计数器占用字节数（4 字节）

/**
 * @brief 读取第 2 页存储的计数器值
 * @param counter 指向存储读取结果的 uint32_t 指针
 * @return 0:成功，1:数据未初始化（首次使用，全 0xFF），2:指针无效
 */
uint8_t eeprom_read_counter(uint32_t *counter)
{
    // 检查指针有效性
    if (counter == NULL) {
        return 2;
    }

    uint8_t buf[COUNTER_SIZE];
    // 从第 2 页偏移 8 字节处读取 4 字节数据
    AT24C128_ReadBuffer(COUNTER_ADDR, buf, COUNTER_SIZE);

    // 检查是否未初始化（EEPROM 初始状态为全 0xFF）
    uint8_t is_uninit = 1;
    for (uint8_t i = 0; i < COUNTER_SIZE; i++) {
        if (buf[i] != 0xFF) {
            is_uninit = 0;
            break;
        }
    }
    if (is_uninit) {
        *counter = 0; // 未初始化时返回默认值 0
        return 1;
    }

    // 将字节数组转换为 uint32_t（小端模式）
    *counter = (uint32_t)buf[0] | 
               (uint32_t)buf[1] << 8 | 
               (uint32_t)buf[2] << 16 | 
               (uint32_t)buf[3] << 24;

    return 0;
}

/**
 * @brief 向第 2 页写入计数器值（自增 1）
 * @return 0:成功，1:写入失败（校验不通过），2:溢出（计数器达到最大值）
 */
uint8_t eeprom_increment_counter(void)
{
    eeprom_Count++;
    uint32_t current_counter = 0;
    // 读取当前计数器值
    uint8_t read_ret = eeprom_read_counter(&current_counter);
    if (read_ret == 2) {
        return 1; // 指针无效，读取失败
    }

    // 检查是否溢出（uint32_t 最大值为 4294967295）
    if (current_counter >= 0xFFFFFFFF) {
        return 2; // 溢出，无法自增
    }

    // 自增计数器
    uint32_t new_counter = current_counter + 1;

    // 将新值转换为字节数组（小端模式）
    uint8_t buf[COUNTER_SIZE];
    buf[0] = (uint8_t)(new_counter & 0xFF);
    buf[1] = (uint8_t)((new_counter >> 8) & 0xFF);
    buf[2] = (uint8_t)((new_counter >> 16) & 0xFF);
    buf[3] = (uint8_t)((new_counter >> 24) & 0xFF);

    // 写入第 2 页偏移 8 字节处
    AT24C128_WritePage(COUNTER_ADDR, buf, COUNTER_SIZE);
    delay_ms(5); // 等待 EEPROM 写入完成

    // 校验写入结果
    uint32_t verify_counter = 0;
    if (eeprom_read_counter(&verify_counter) != 0 || verify_counter != new_counter) {
        return 1; // 校验失败，写入异常
    }

		_log(LOG_DEBUG, "计数器自增成功：%u → %u", current_counter, new_counter);
    return 0;
}

/**
 * @brief 从EEPROM读取存储的时间
 * @return 读取到的时间（time_t类型），0表示未存储时间
 */
time_t Read_eeprom_Time(void)
{
    uint8_t time_buffer[4];
    AT24C128_ReadBuffer(TIME_PAGE1_ADDR, time_buffer, 4);
    // 检查是否为未初始化状态（全0xFF）
    if (time_buffer[0] == 0xFF && time_buffer[1] == 0xFF &&
        time_buffer[2] == 0xFF && time_buffer[3] == 0xFF) {
        return 0;  // 未存储时间
    }
    return *(time_t*)time_buffer;
}

/**
 * @brief 向EEPROM写入时间
 * @param current_time 要存储的时间（time_t类型）
 */
void Write_eeprom_Time(time_t current_time)
{
    uint8_t time_buffer[4];
    *(time_t *)time_buffer = current_time;  // 转换为字节数组
    AT24C128_WritePage(TIME_PAGE1_ADDR, time_buffer, 4);
    delay_ms(5);  // 等待写入完成
}

/**
 *@brief 自动更新时间戳判断
 *@return 1:应该更新时间戳，0:不用更新时间戳
*/
uint8_t Auto_Update_TimeStamp(void)
{
    time_t stored_time = Read_eeprom_Time();
    time_t current_time = convert_to_timestamp();

    if (stored_time == 0) {
        // EEPROM中无存储时间，需写入当前时间
        Write_eeprom_Time(current_time);
        return 1;
    }
    else if (current_time - stored_time > (DF_UPDATE_TIME * 24 * 3600)) {
        // 超过设定天数，更新时间戳
        Write_eeprom_Time(current_time);
        return 1;
    }
    return 0;
}

// 设备参数存储结构体（含校验和，防止数据损坏）
typedef struct {
    uint8_t led_state;              // LED状态（0-4）
    uint8_t U1_BaudRate;            // U1波特率（0-6）
    uint8_t U3_BaudRate;            // U3波特率（0-6）
    __IO uint8_t LOG_Detailed_inf;  // 日志详细程度（0-3）
    uint8_t check_sum;              // 校验和（led_state ^ U1_BaudRate ^ U3_BaudRate ^ LOG_Detailed_inf）
} DeviceParams_t;

extern __IO uint8_t DTURX_Data[1];		   		// 接收字节缓存

/**
 * @brief 计算参数校验和（防止EEPROM数据损坏）
 * @param params 参数结构体指针
 * @return 校验和（led_state ^ U1_BaudRate ^ U3_BaudRate）
 */
static uint8_t calc_param_checksum(const DeviceParams_t *params)
{
    if (params == NULL) return 0;
    // 校验和计算
    return params->led_state ^ params->U1_BaudRate ^ params->U3_BaudRate ^ params->LOG_Detailed_inf;
}

/**
 * @brief 上电读取EEPROM第三页的设备参数
 * @note 1. 读取后对比默认值，不符则打警告日志
 *       2. 即使数据无效/不符默认值，仍应用读取到的值
 * @return 0:执行完成，1:参数校验失败，2:读取异常
 */
uint8_t eeprom_read_device_params(void)
{
    DeviceParams_t read_params;
    uint8_t read_buf[sizeof(DeviceParams_t)] = {0};
    uint8_t ret = 0;

    // 1. 从EEPROM第三页读取参数数据
    AT24C128_ReadBuffer(PARAM_PAGE_NUM, read_buf, sizeof(DeviceParams_t));
    memcpy(&read_params, read_buf, sizeof(DeviceParams_t));

    // 2. 校验数据有效性（校验和）
    uint8_t calc_sum = calc_param_checksum(&read_params);
    if (read_params.check_sum != calc_sum) {
        _log(LOG_ERROR, "EEPROM参数校验失败！读取校验和:0x%02X, 计算值:0x%02X", read_params.check_sum, calc_sum);
        return 1;
    }

    // 3. 检查参数范围（防止非法值）
    if (read_params.led_state > 4) {
        _log(LOG_ERROR, "EEPROM LED状态超出范围(%d)，默认值:%d", read_params.led_state, DEFAULT_LED_STATE);
        return 1;
    }
    if (read_params.U1_BaudRate > 6) {
        _log(LOG_ERROR, "EEPROM U1波特率超出范围(%d)，默认值:%d", read_params.U1_BaudRate, DEFAULT_U1_BAUDRATE);
        return 1;
    }
    if (read_params.U3_BaudRate > 6) {
        _log(LOG_ERROR, "EEPROM U3波特率超出范围(%d)，默认值:%d", read_params.U3_BaudRate, DEFAULT_U3_BAUDRATE);
        return 1;
    }
    if (read_params.LOG_Detailed_inf > 3) {
        _log(LOG_ERROR, "EEPROM 日志等级超出范围(%d)，默认值:%d", read_params.LOG_Detailed_inf, DEFAULT_LOG_DETAIL);
        return 1;
    }

    // 4. 对比默认值
    if (read_params.led_state != DEFAULT_LED_STATE) {
        _log(LOG_WARN, "EEPROM LED状态与默认值不符！读取值:%d, 默认值:%d", read_params.led_state, DEFAULT_LED_STATE);
    }
    if (read_params.U1_BaudRate != DEFAULT_U1_BAUDRATE) {
        _log(LOG_WARN, "EEPROM U1波特率与默认值不符！读取值:%d, 默认值:%d", read_params.U1_BaudRate, DEFAULT_U1_BAUDRATE);
    }
    if (read_params.U3_BaudRate != DEFAULT_U3_BAUDRATE) {
        _log(LOG_WARN, "EEPROM U3波特率与默认值不符！读取值:%d, 默认值:%d", read_params.U3_BaudRate, DEFAULT_U3_BAUDRATE);
    }
    if (read_params.LOG_Detailed_inf != DEFAULT_LOG_DETAIL) {
        _log(LOG_WARN, "EEPROM 日志等级与默认值不符！读取值:%d, 默认值:%d", read_params.LOG_Detailed_inf, DEFAULT_LOG_DETAIL);
    }

    // 5. 应用读取到的参数
    led_state = read_params.led_state;
    U1_BaudRate = read_params.U1_BaudRate;
    U3_BaudRate = read_params.U3_BaudRate;
    LOG_Detailed_inf = read_params.LOG_Detailed_inf; 

    // 6. 配置串口波特率
    int BaudRate1 = 0;
    switch (U1_BaudRate)
    {
    case 0: BaudRate1 = 4800; break;
    case 1: BaudRate1 = 9600; break;
    case 2: BaudRate1 = 14400; break;
    case 3: BaudRate1 = 19200; break;
    case 4: BaudRate1 = 38400; break;
    case 5: BaudRate1 = 115200; break;
    case 6: BaudRate1 = 256000; break;
    default:
        _log(LOG_ERROR, "U1波特率参数非法，默认115200");
        BaudRate1 = 115200;
        break;
    }
    Runing_UARTSET_B(&huart1, BaudRate1, NULL);

    int BaudRate3 = 0;
    switch(U3_BaudRate)
    {
    case 0: BaudRate3 = 4800; break;
    case 1: BaudRate3 = 9600; break;
    case 2: BaudRate3 = 14400; break;
    case 3: BaudRate3 = 19200; break;
    case 4: BaudRate3 = 38400; break;
    case 5: BaudRate3 = 115200; break;
    case 6: BaudRate3 = 256000; break;
    default:
        _log(LOG_ERROR, "U3波特率参数非法，默认115200");
        BaudRate3 = 115200;
        break;
    }
    Runing_UARTSET_B(&huart3, BaudRate3, (uint8_t *)DTURX_Data);

    // 7. 打印最终生效的参数
    _log(LOG_DEBUG, "LED 参数意义- 0：led熄灭，1：常量，2：闪烁，3:信息交互时亮，4：出现警告以上错误时常亮");
    _log(LOG_DEBUG, "日志等级 参数意义- 0：关闭DEBUG级别输出，1：简略DEBUG、WARN信息，2：简略DEBUG信息，3：不省略");
    _log(LOG_DEBUG, "EEPROM参数读取完成 - LED:%d, U1波特率:%d, U3波特率:%d, 日志等级:%d",
         led_state, BaudRate1, BaudRate3, LOG_Detailed_inf);

    return ret;
}

/**
 * @brief 运行时将当前设备参数保存到EEPROM第三页
 * @note 写入后自动校验，确保数据一致性
 * @return 0:保存成功，1:保存失败（校验不通过），2:参数非法
 */
uint8_t eeprom_save_device_params(void)
{
    // 1. 检查参数合法性
    if (led_state > 4 || U1_BaudRate > 6 || U3_BaudRate > 6 || LOG_Detailed_inf > 3) {
        _log(LOG_ERROR, "EEPROM参数保存失败！参数非法 - LED:%d, U1:%d, U3:%d, 日志等级:%d", 
             led_state, U1_BaudRate, U3_BaudRate, LOG_Detailed_inf);
        return 2;
    }

    // 2. 构建参数结构体并计算校验和
    DeviceParams_t save_params = {
        .led_state = led_state,
        .U1_BaudRate = U1_BaudRate,
        .U3_BaudRate = U3_BaudRate,
        .LOG_Detailed_inf = LOG_Detailed_inf, // 赋值日志详细程度
        .check_sum = calc_param_checksum(&save_params)
    };

    // 3. 转换为字节数组并写入EEPROM
    uint8_t save_buf[sizeof(DeviceParams_t)] = {0};
    memcpy(save_buf, &save_params, sizeof(DeviceParams_t));
    AT24C128_WritePage(PARAM_PAGE_NUM, save_buf, sizeof(DeviceParams_t));
    delay_ms(5); // 等待EEPROM写入完成

    // 4. 校验写入结果
    DeviceParams_t verify_params;
    AT24C128_ReadBuffer(PARAM_PAGE_NUM, save_buf, sizeof(DeviceParams_t));
    memcpy(&verify_params, save_buf, sizeof(DeviceParams_t));

    if (memcmp(&save_params, &verify_params, sizeof(DeviceParams_t)) != 0) {
        _log(LOG_ERROR, "EEPROM参数保存失败！写入值与读取值不一致");
        return 1;
    }

    // 5. 保存成功日志
    _log(LOG_DEBUG, "EEPROM参数保存成功 - LED:%d, U1波特率:%d, U3波特率:%d, 日志等级:%d", 
         led_state, U1_BaudRate, U3_BaudRate, LOG_Detailed_inf);

    return 0;
}


