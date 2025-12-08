#ifndef __KEY_H
#define __KEY_H

#define KEY_COUNT				1

#define KEY0 					0

#define KEY_HOLD				0x01
#define KEY_DOWN				0x02    // 按下
#define KEY_UP					0x04    // 抬起
#define KEY_SINGLE				0x08    // 单击
#define KEY_DOUBLE				0x10    // 双击
#define KEY_LONG				0x20    // 长按
#define KEY_REPEAT				0x40    // 连续长按

uint8_t Key_Check(uint8_t n, uint8_t Flag);
void Key_Tick(void);

#endif
