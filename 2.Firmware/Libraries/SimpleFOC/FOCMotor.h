#ifndef FOCMOTOR_H
#define FOCMOTOR_H

#include "foc_utils.h"
/******************************************************************************/
/**
 *  Motiron control type
 */
typedef enum
{
	Type_torque,   //!< Torque control
	Type_velocity, //!< Velocity motion control
	Type_angle,	   //!< Position/angle motion control
	Type_velocity_openloop,
	Type_angle_openloop
} MotionControlType;

/**
 *  Motiron control type
 */
typedef enum
{
	Type_voltage,	 //!< Torque control using voltage
	Type_dc_current, //!< Torque control using DC current (one current magnitude)
	Type_foc_current //!< torque control using dq currents
} TorqueControlType;

extern TorqueControlType torque_controller;
extern MotionControlType controller;
/******************************************************************************/
// volatile：跨「1kHz 任务(move)」与「20kHz 中断(loopFOCISR)」共享的变量，防止编译器缓存到寄存器读旧值
extern volatile float shaft_angle; //!< current motor angle (中断写/任务读)
extern volatile float electrical_angle; // (中断写/任务读)
extern volatile float shaft_velocity; // (中断写/任务读)
extern volatile float current_sp; // (任务写/中断读)
extern float shaft_velocity_sp;
extern float shaft_angle_sp;
extern volatile DQVoltage_s voltage; // (电压模式任务写/中断读；电流模式中断写)
extern volatile DQCurrent_s current; // (中断写/任务读)

extern float sensor_offset;
extern float zero_electric_angle;
/******************************************************************************/
float shaftAngle(void);
float shaftVelocity(void);
float electricalAngle(void);
/******************************************************************************/

#endif
