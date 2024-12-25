#ifndef __MYADC_H
#define __MYADC_H
#include "sys.h"

#define ADC_BATTERY ADC_Channel_2
#define ADC_SENSE_A ADC_Channel_14
#define ADC_SENSE_B ADC_Channel_15

void MyADC_Init(void); // ADC通道初始化

uint16_t MyADC_GetValue(uint8_t channel); // 获得某个通道的数值

uint16_t MyADC_GetValue_Average(uint8_t channel, uint8_t times); // 得到某个通道给定次数采样的平均值

float getBetteryVolt(void); // 获取当前电池电压值

// 获得ADC值，在FOC电流采样中使用
unsigned short analogRead(unsigned char channel);

// function reading an ADC value and returning the read voltage
float _readADCVoltageInline(unsigned char ch);

#endif
