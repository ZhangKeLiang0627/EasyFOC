#ifndef __DELAY_H
#define __DELAY_H
#include <sys.h>

/******************************************************************************/
void delay_init(uint8_t SYSCLK);
// void systick_CountInit(void);

uint32_t _micros(void);
uint32_t millis(void);

void delay_us(uint32_t nus);
void delay_ms(uint32_t nms);

// DWT 周期计数器（Cortex-M4F CYCCNT），用于精确测量代码执行时间
void DWT_Init(void);      // 使能 DWT->CYCCNT
uint32_t DWT_GetCycle(void); // 读取当前周期计数

// void delay_us(unsigned long nus);
// void delay_ms(unsigned long nms);
/******************************************************************************/

#endif
