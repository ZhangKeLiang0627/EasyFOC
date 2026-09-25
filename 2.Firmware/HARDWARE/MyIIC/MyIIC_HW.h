#ifndef __MYIIC_HW_H
#define __MYIIC_HW_H
#include "stm32f4xx.h"

/*
 * 硬件 I2C 库（I2C3 外设）
 * 与 MyIIC_SW（软件 I2C）接口对齐，供 AS5600 磁编码器在软/硬 I2C 之间切换。
 *
 * 引脚复用（与软件 I2C 完全一致，无需改硬件）：
 *   SCL = PA8  ->  I2C3_SCL (AF4 = GPIO_AF_I2C3)
 *   SDA = PB4  ->  I2C3_SDA (AF9 = GPIO_AF9_I2C3)
 *
 * 注意：PB4 默认是 JTAG 的 NJTRST，配置为 AF9 后自动释放；
 *       不影响 SWD 调试（PA13/PA14 不受影响）。
 */

void MyIIC_HW_Init(void);
uint8_t MyIIC_HW_Write_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address, uint8_t REG_data);
uint8_t MyIIC_HW_Read_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address);
uint8_t MyIIC_HW_Write_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf);
uint8_t MyIIC_HW_Read_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf);

#endif
