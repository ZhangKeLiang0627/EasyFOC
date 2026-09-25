
#include "FOCuser_Inc.h"

/******************************************************************************/
volatile float shaft_angle; //!< current motor angle
volatile float electrical_angle;
volatile float shaft_velocity;
volatile float current_sp;
float shaft_velocity_sp;
float shaft_angle_sp;
volatile DQVoltage_s voltage;
volatile DQCurrent_s current;

TorqueControlType torque_controller;
MotionControlType controller;

float sensor_offset = 0;
float zero_electric_angle;
/******************************************************************************/
// shaft angle calculation
float shaftAngle(void)
{
  // if no sensor linked return previous value ( for open loop )
  // if(!sensor) return shaft_angle;
  return sensor_direction * getAngle() - sensor_offset;
}
// shaft velocity calculation
// 注意：只能用 loopFOC() 更新好的全局 shaft_angle；任务里再调 getAngle()/getVelocity()
// 会与中断并发读同一编码器，导致速度估算失效。
float shaftVelocity(void)
{
  static uint32_t ts_prev = 0;
  static float ang_prev = 0.0f;
  uint32_t now_us = _micros();
  float Ts, vel;

  Ts = (now_us - ts_prev) * 1e-6f;
  if (Ts <= 0 || Ts > 0.5f)
    Ts = 1e-3f;

  vel = (shaft_angle - ang_prev) / Ts;

  ang_prev = shaft_angle;
  ts_prev = now_us;

  return LPFoperator(&LPF_velocity, vel);
}
/******************************************************************************/
float electricalAngle(void)
{
  return _normalizeAngle((shaft_angle + sensor_offset) * pole_pairs - zero_electric_angle);
}
/******************************************************************************/
