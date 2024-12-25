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
