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

// void delay_us(unsigned long nus);
// void delay_ms(unsigned long nms);
/******************************************************************************/

#endif
