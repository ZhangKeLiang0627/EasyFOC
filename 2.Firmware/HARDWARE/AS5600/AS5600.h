#ifndef __AS5600_H
#define __AS5600_H

#ifdef __cplusplus
extern "C"
{
#endif
	
#include "stm32f4xx.h"

/* define AS5600 REGISTER */	
#define AS5600_RAW_ANGLE_REGISTER1  0x0C // High-order
#define AS5600_RAW_ANGLE_REGISTER2  0x0D // Low-order
	
/* define AS5600 IIC address */
#define AS5600_IIC_ADDR 0x36	

/*
 * I2C 驱动方式选择（软/硬二选一，引脚完全一致，无需改硬件）：
 *   0 = 软件 I2C（MyIIC_SW，GPIO 模拟时序）
 *   1 = 硬件 I2C3（PA8=SCL / PB4=SDA，外设收发，400kHz 快速模式）
 * 默认使用硬件 I2C：软件 I2C 单次读约 300us，在 10kHz 电流环中断里会占满 CPU。
 */
#define AS5600_USE_HW_I2C 1

#define AS5600_CPR 4096 //12bit Resolution
	
void AS5600_Write_Byte(uint8_t addr, uint8_t dat);
void AS5600_Write_MultiBytes(uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf);
uint8_t AS5600_Read_Byte(uint8_t addr);
uint8_t AS5600_Read_MultiBytes(uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf);
	
uint8_t AS5600_Init(void);
uint16_t AS5600_GetRawAngle(void);

#ifdef __cplusplus
}
#endif

#endif
