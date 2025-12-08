#ifndef __AT24C64_H
#define __AT24C64_H

#include "main.h"


#define AT24C128_ADDR    0xA0      // 器件写地址 (1010 0000)
#define AT24C128_PAGE_SIZE 64      // 页大小(字节)
#define AT24C128_MAX_ADDR 0x3FFF   // 最大地址(16K-1)

uint8_t AT24C128_IsReady(void);

void AT24C128_WriteByte(uint16_t addr, uint8_t data);

void AT24C128_WritePage(uint16_t addr, uint8_t* data, uint16_t len);

uint8_t AT24C128_ReadByte(uint16_t addr);

void AT24C128_ReadBuffer(uint16_t addr, uint8_t* buf, uint16_t len);



#endif // __AT24C64_H


