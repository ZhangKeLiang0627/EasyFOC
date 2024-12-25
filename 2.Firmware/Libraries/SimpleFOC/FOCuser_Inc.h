#ifndef __FOCUSER_INC_H
#define __FOCUSER_INC_H

/* Includes ------------------------------------------------------------------ */

// Core
#include "stm32f4xx.h"
#include "usart.h"
#include "delay.h"
#include "MyADC.h"

// SimpleFOC
#include "foc_utils.h"
#include "FOCMotor.h"
#include "BLDCmotor.h"
#include "FOCBaseConfig.h"

#include "MagneticSensor.h"

#include "CurrentSense.h"
#include "InlineCurrentSense.h"

#include "lowpass_filter.h"
#include "pid.h"

// Senser
#include "AS5600.h"

/* Defines ------------------------------------------------------------------ */

#define M1_TIMx TIM3
#define M1_Enable() GPIO_SetBits(GPIOC, GPIO_Pin_14)     // 高电平使能
#define M1_Disable() GPIO_ResetBits(GPIOC, GPIO_Pin_14); // 低电平解除

#define PWM_Period (1680 * 2)                            // 1680 * 2 -> 25kHz // TIM3 -> 84MHz

#endif
