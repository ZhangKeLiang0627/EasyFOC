#ifndef __CURRENTSENSE_H
#define __CURRENTSENSE_H

/******************************************************************************/
#include "foc_utils.h"
/******************************************************************************/
float getDCCurrent(float motor_electrical_angle);
DQCurrent_s getFOCCurrents(float angle_el);
float getDCCurrentISR(float motor_electrical_angle); // 中断版：读注入组
DQCurrent_s getFOCCurrentsISR(float angle_el);       // 中断版：读注入组
/******************************************************************************/

#endif
