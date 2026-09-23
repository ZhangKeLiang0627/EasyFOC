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

// === 注入组电流采样（20kHz 中断里使用，与规则组/电池电压解耦）===
// F401 注入组无 TIM3_TRGO 触发源，采用「定时器下溢中断 + 软件启动注入转换」，
// 采样点对齐由 TIM3 中心对齐下溢（零矢量）时刻保证。
void MyADC_StartInjected(void);             // 软件启动注入组转换（CH14=PC4 A相, CH15=PC5 B相）
uint16_t MyADC_GetInjectedValue1(void);     // 读 JDR1（A相 raw）
uint16_t MyADC_GetInjectedValue2(void);     // 读 JDR2（B相 raw）

#endif
