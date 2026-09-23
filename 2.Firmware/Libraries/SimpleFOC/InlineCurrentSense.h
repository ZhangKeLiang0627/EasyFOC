#ifndef __INLINE_CS_LIB_H
#define __INLINE_CS_LIB_H

#include "foc_utils.h"

/******************************************************************************/
void InlineCurrentSense(float _shunt_resistor, float _gain, int _pinA, int _pinB, int _pinC);
void InlineCurrentSense_Init(void);
PhaseCurrent_s getPhaseCurrents(void);
PhaseCurrent_s getPhaseCurrentsISR(void); // 中断版：读注入组 JDR1/JDR2，不再走 analogRead 阻塞轮询
/******************************************************************************/

#endif
