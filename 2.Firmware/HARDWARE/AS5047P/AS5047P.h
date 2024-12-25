#ifndef __AS5047P_H
#define __AS5047P_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx.h"

/* define AS5047P REGISTER */
#define AS5047P_REGISTER_NOP 0x0000      // No operation
#define AS5047P_REGISTER_ERRFL 0x0001    // Error register
#define AS5047P_REGISTER_PROG 0x0003     // Programming register
#define AS5047P_REGISTER_DIAAGC 0x3FFC   // Diagnostic and AGC
#define AS5047P_REGISTER_MAG 0x3FFD      // CORDIC magnitude
#define AS5047P_REGISTER_ANGLEUNC 0x3FFE // Measured angle without dynamic angle error compensation
#define AS5047P_REGISTER_ANGLECOM 0x3FFF // Measured angle with dynamic angle error compensation

#define AS5047P_REGISTER_ZPOSM 0x0016
#define AS5047P_REGISTER_ZPOSL 0x0017
#define AS5047P_REGISTER_SETTINGS1 0x0018
#define AS5047P_REGISTER_SETTINGS2 0x0019

#define AS5047P_CPR 16384 // 14bit

/* funtion AS5047P  */
uint8_t AS5047P_Init(void);
uint16_t AS5047P_GetRawAngle(void);

#ifdef __cplusplus
}
#endif

#endif
