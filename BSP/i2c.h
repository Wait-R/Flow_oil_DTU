#ifndef __I2C_H
#define __I2C_H

#include "main.h"
#include "gpio.h"

/* IIC所有操作函数 */
void iic_init(void);                /* 初始化IIC的IO口 */
void iic_start(void);               /* 发送IIC开始信号 */
void iic_stop(void);                /* 发送IIC停止信号 */
void iic_ack(void);                 /* IIC发送ACK信号 */
void iic_nack(void);                /* IIC不发送ACK信号 */
uint8_t iic_wait_ack(void);         /* IIC等待ACK信号 */
void iic_send_byte(uint8_t data);   /* IIC发送一个字节 */
uint8_t iic_read_byte(uint8_t ack); /* IIC读取一个字节 */

#endif



