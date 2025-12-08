#include "eeprom_drive.h"
#include "at24c64.h"  // 使用AT24C128 EEPROM驱动
#include "i2c.h"
#include "delay.h"
#include "stdio.h"
#include "Task.h"
#include "string.h"
#include "Capture_Traffic.h"
#include <math.h>

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
- 第1页：存储时间戳（以弃用）
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
        printf("已接收请求\r\n");

        // 查找最后一条有数据的索引
        int16_t last_index = -1;
        for (int16_t i = MAX_DATA_COUNT - 1; i >= 0; i--) {
            if (is_data_used(i)) {
                last_index = i;
                break;
            }
        }
        
        if (last_index == -1) {
            printf("无数据可读取\r\n");
            return;
        }
        
        // 读取数据
        OilDataWithTime read_data;
        uint16_t storage_addr = data_index_to_addr(last_index);
        x24Cxx_Read_float(storage_addr, &read_data);

        // 发送到上传队列
        if (xQueueSend(UploadQueueHandle, &read_data, 0) != pdPASS) {
            printf("错误：数据无法发送到上传队列\r\n");
            return;
        }
        else {
            printf("数据:%.3f 已发送到上传队列\r\n", read_data.oil_flow);
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
                
                printf("成功上传,本地删除,剩余：%d\r\n", get_Data_Page_Count());
            }
            else {
                printf("上传失败\r\n");
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
    printf("=== EEPROM数据信息 ===\n");
    printf("总数据量: %d/%d\n", eeprom_data_count, MAX_DATA_COUNT);
    
    uint16_t count = 0;
    // 遍历所有数据位置，打印有效数据
    for (uint16_t i = 0; i < MAX_DATA_COUNT; i++) {
        if (is_data_used(i)) {
            OilDataWithTime data;
            x24Cxx_Read_float(data_index_to_addr(i), &data);
            printf("索引%d: 地址0x%04X, 数据: %.2f | %lu\r\n",
                   i, data_index_to_addr(i), data.oil_flow, data.time_stamp);
            count++;
            if (count >= eeprom_data_count) break;  // 已打印所有数据，提前退出
        }
    }
    if (count == 0) {
        printf("没有找到数据\n");
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

/**
 * @brief 从第2页读取存储的double类型数据
 * @param data 存储读取结果的double指针
 * @return 0:成功，1:数据未初始化（未写入过），2:指针无效, 3:数据非法
 */
uint8_t eeprom_read_double_from_page2(double *data)
{
    if (data == NULL) {
        return 2;
    }

    uint8_t buf[sizeof(double)];
    // 读取第2页起始地址的8字节数据
    AT24C128_ReadBuffer(COEFFICIENT_PAGE_ADDR, buf, sizeof(double));

    // 严格判断是否全为0xFF（未初始化）
    uint8_t is_uninit = 1;
    for (uint8_t i = 0; i < sizeof(double); i++) {
        if (buf[i] != 0xFF) {
            is_uninit = 0;
            break;
        }
    }
    if (is_uninit) {
        *data = 0.0; // 避免data残留脏数据
        return 1;
    }

    // 将字节数组转换为double后，额外判断是否为nan或inf
    memcpy(data, buf, sizeof(double));
    if (isnan(*data) || isinf(*data)) {
        *data = 0.0;
        return 3; // 新增返回值：数据非法（nan/inf）
    }

    return 0;
}

/**
 * @brief 擦除第2页存储的double类型数据（填充0xFF）
 * @return 0:成功
 */
uint8_t eeprom_erase_double_from_page2(void)
{
    uint8_t blank[sizeof(double)];
    memset(blank, 0xFF, sizeof(double));  // EEPROM擦除值为0xFF

    // 向第2页起始地址写入8字节0xFF，覆盖原double数据
    AT24C128_WritePage(COEFFICIENT_PAGE_ADDR, blank, sizeof(double));
    delay_ms(5);  // 等待EEPROM写入完成

    return 0;
}

/**
 * @brief 向第2页写入double类型数据
 * @param data 要存储的double数据
 * @return 0:成功，1:写入失败（校验不通过）
 */
uint8_t eeprom_write_double_to_page2(double data)
{
    uint8_t buf[sizeof(double)];
    // 将double转换为字节数组
    memcpy(buf, &data, sizeof(double));

    // 向第2页起始地址写入8字节（double占8字节）
    AT24C128_WritePage(COEFFICIENT_PAGE_ADDR, buf, sizeof(double));
    delay_ms(5);  // 等待EEPROM写入完成

    // 数据校验：写入后立即读取，确保数据正确
    double read_back;
    if (eeprom_read_double_from_page2(&read_back) != 0) {
        return 1;  // 读取校验失败
    }
    // 对比写入值和读取值（允许浮点数微小误差）
    if (fabs(read_back - data) > 1e-6) {
        return 1;  // 数据不一致，写入失败
    }

    return 0;  // 成功
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

    // printf("计数器自增成功：%u → %u\r\n", current_counter, new_counter);
    return 0;
}

// void eeprom_Coefficient_Init(void)
// {
//     vTaskDelay(10); // 等待系统稳定
//     double stored_coefficient = 0;
//     uint8_t result = eeprom_read_double_from_page2(&stored_coefficient);
//     switch(result) {
//         case 0:
//             coefficient = stored_coefficient;
//             printf("读取流量系数: %.15f\r\n", coefficient);
//             break;
//         case 1:
//             coefficient = COEFFICIENT; // 默认系数
//             printf("流量系数未设置，使用默认值: %.15f\r\n", coefficient);
//             eeprom_write_double_to_page2(COEFFICIENT); // 初始化流量系数存储
//             break;
//         case 2:
//             coefficient = COEFFICIENT;
//             printf("读取流量系数失败：指针无效，使用默认值: %.15f\r\n", coefficient);
//             break;
//         case 3:
//             coefficient = COEFFICIENT;
//             printf("读取流量系数非法（nan/inf），使用默认值: %.15f\r\n", coefficient);
//             // 可选：擦除非法数据，避免下次读取仍出错
//             eeprom_erase_double_from_page2();
//             eeprom_write_double_to_page2(COEFFICIENT); // 初始化流量系数存储
//             break;
//         default:
//             coefficient = COEFFICIENT;
//             printf("读取流量系数异常，使用默认值: %.15f\r\n", coefficient);
//             break;
//     }
//     vTaskDelay(100); // 等待系统稳定
// }
