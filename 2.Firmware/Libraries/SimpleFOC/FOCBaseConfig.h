#ifndef __FOCBASECONFIG_H
#define __FOCBASECONFIG_H

/* Includes ------------------------------------------------------------------ */

#include "stm32f4xx.h"

void TIM3_PWM_Init(u16 arr);
void FOC_GPIO_Config(void);
void TIM10_Count_Init(void);
void EasyFOC_Init(void);

/* Defines ------------------------------------------------------------------ */

#endif
