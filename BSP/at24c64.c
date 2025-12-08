#include "i2c.h"
#include "at24c64.h"

// 检查EEPROM是否就绪
uint8_t AT24C128_IsReady(void)
{
//    iic_start();
//    uint8_t ack = iic_wait_ack();  // 发送设备地址检测ACK
//    iic_stop();
//    return (ack == 0);  // 返回1:就绪, 0:忙碌
	return 1;
}

// 单字节写入
void AT24C128_WriteByte(uint16_t addr, uint8_t data) {
    // 地址校验
    if(addr > AT24C128_MAX_ADDR) return;
    
    while(!AT24C128_IsReady()); // 等待设备就绪
    
    iic_start();
    iic_send_byte(AT24C128_ADDR);  // 发送写命令
    iic_wait_ack();
    iic_send_byte(addr >> 8);      // 发送地址高字节
    iic_wait_ack();
    iic_send_byte(addr & 0xFF);    // 发送地址低字节
    iic_wait_ack();
    iic_send_byte(data);           // 发送数据
    iic_wait_ack();
    iic_stop();
}

// 页写入(最多64字节)
void AT24C128_WritePage(uint16_t addr, uint8_t *data, uint16_t len) {
    // 边界检查
    if(len == 0 || len > AT24C128_PAGE_SIZE) return;
    if(addr > AT24C128_MAX_ADDR) return;
    
    // 页边界检查
    uint16_t page_boundary = (addr & ~(AT24C128_PAGE_SIZE - 1)) + AT24C128_PAGE_SIZE;
    if((addr + len) > page_boundary) {
        len = page_boundary - addr; // 自动截断到页尾
    }
    
    while(!AT24C128_IsReady()); // 等待设备就绪
    
    iic_start();
    iic_send_byte(AT24C128_ADDR);
    iic_wait_ack();
    iic_send_byte(addr >> 8);      // 地址高字节
    iic_wait_ack();
    iic_send_byte(addr & 0xFF);    // 地址低字节
    iic_wait_ack();
    
    // 连续写入数据
    for(uint16_t i = 0; i < len; i++) {
        iic_send_byte(data[i]);
        if(iic_wait_ack()) break; // 出错中止
    }
    
    iic_stop();
}

// 随机地址读取
uint8_t AT24C128_ReadByte(uint16_t addr) {
    if(addr > AT24C128_MAX_ADDR) return 0xFF;
    
    while(!AT24C128_IsReady()); // 等待设备就绪
    
    iic_start();
    iic_send_byte(AT24C128_ADDR);  // 写命令
    iic_wait_ack();
    iic_send_byte(addr >> 8);      // 地址高字节
    iic_wait_ack();
    iic_send_byte(addr & 0xFF);    // 地址低字节
    iic_wait_ack();
    
    iic_start(); // 重复起始条件
    iic_send_byte(AT24C128_ADDR | 0x01); // 读命令
    iic_wait_ack();
    
    uint8_t data = iic_read_byte(0); // 读取1字节后发送NACK
    iic_stop();
    
    return data;
}

// 顺序读取多个字节
void AT24C128_ReadBuffer(uint16_t addr, uint8_t *buf, uint16_t len) {
    if(len == 0 || addr > AT24C128_MAX_ADDR) return;
    
    while(!AT24C128_IsReady()); // 等待设备就绪
    
    iic_start();
    iic_send_byte(AT24C128_ADDR);  // 写命令
    iic_wait_ack();
    iic_send_byte(addr >> 8);      // 地址高字节
    iic_wait_ack();
    iic_send_byte(addr & 0xFF);    // 地址低字节
    iic_wait_ack();
    
    iic_start(); // 重复起始条件
    iic_send_byte(AT24C128_ADDR | 0x01); // 读命令
    iic_wait_ack();
    
    // 连续读取数据
    for(uint16_t i = 0; i < len; i++) {
        buf[i] = iic_read_byte(i == (len-1) ? 0 : 1); // 最后字节发NACK
    }
    
    iic_stop();
}


