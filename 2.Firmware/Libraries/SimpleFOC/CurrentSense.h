#ifndef __CURRENTSENSE_H
#define __CURRENTSENSE_H

/******************************************************************************/
#include "foc_utils.h"
/******************************************************************************/
float getDCCurrent(float motor_electrical_angle);
DQCurrent_s getFOCCurrents(float angle_el);
/******************************************************************************/

#endif
