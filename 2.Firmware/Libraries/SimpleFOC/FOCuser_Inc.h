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
#include "AS5047P.h"

/* Defines ------------------------------------------------------------------ */

#define M1_TIMx TIM3
#define M1_Enable() GPIO_SetBits(GPIOC, GPIO_Pin_14)     // 高电平使能
#define M1_Disable() GPIO_ResetBits(GPIOC, GPIO_Pin_14); // 低电平解除

/* PWM频率: TIM3时钟84MHz, 中心对齐, 实际频率 f = 84MHz / (2 * ARR) */
#define PWM_Period (2100)                            // 20kHz: ARR = 2100 -> 84MHz / (2 * 2100) = 20.000kHz
// #define PWM_Period (1680)                         // ARR = 1680 -> 84MHz / (2 * 1680) = 25.000kHz

/* 电流环中断采样周期: 10kHz -> 100us（PWM 仍为 20kHz，每 2 个 PWM 周期采样一次） */
#define FOC_ISR_TS (1.0e-4f)                          // 10kHz 电流环固定采样周期 = 100us，用于 PID/LPF 固定 dt 版本

#endif
