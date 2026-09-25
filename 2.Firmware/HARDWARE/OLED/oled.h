#ifndef __OLED_H
#define __OLED_H 

#include "sys.h"
#include "stdlib.h"	


#define I2C_SPEED           1000000//400kHz
#define OWN_ADDRESS          0X77
#define OLED_ADDRESS         0X78 
#define OLED_I2C             I2C1

#define I2C_TIMEOUT         100000  // I2C 忙等超时阈值（约几十 ms），防止被 20kHz 中断打断后锁死


void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);

void I2C_WriteByte(uint8_t addr,uint8_t data);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);
void OLED_DrawLine(u8 x1,u8 y1,u8 x2,u8 y2,u8 mode);
void OLED_DrawCircle(u8 x,u8 y,u8 r);
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size1,u8 mode);
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 size1,u8 mode);
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size1,u8 mode);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t* str, uint8_t size1, uint8_t mode);
void OLED_ShowPicture(u8 x,u8 y,u8 sizex,u8 sizey,u8 BMP[],u8 mode);
void OLED_Init(void);

void WriteCmd(unsigned char I2C_Command);
void WriteDat(unsigned char I2C_Data);

// I2C 超时保护：等待事件/busy 释放，超时自动复位 I2C 总线并返回非 0
void I2C1_BusRecover(void);
uint8_t I2C1_WaitEvent(uint32_t event, uint32_t timeout);
uint8_t I2C1_WaitBusyFree(uint32_t timeout);

void OLED_PartialRefresh(uint8_t x1,uint8_t x2,uint8_t y1,uint8_t y2);
void OLED_PartialRefreshForBuff(uint8_t x1,uint8_t x2,uint8_t y1,uint8_t y2);
#endif

