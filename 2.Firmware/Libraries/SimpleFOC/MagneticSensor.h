#ifndef __MAGNETICSENSOR_H
#define __MAGNETICSENSOR_H

/* Include ------------------------------------------------------------------ */
#include "FOCuser_Inc.h"

/* Defines ------------------------------------------------------------------ */
#define MAGNETIC_SENSOR_AS5600 0
#define MAGNETIC_SENSOR_AS5047P 1

/* Functions ------------------------------------------------------------------ */
uint8_t MagneticSensor_Init(void);
uint8_t MagneticSensor_OptionSelect(uint8_t option);
float getAngle(void);
float getVelocity(void);

#endif
