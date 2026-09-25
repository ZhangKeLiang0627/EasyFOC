
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
// 注意：电流环 loopFOC() 已搬进 10kHz 中断，中断里会不断读编码器刷新 shaft_angle。
// 因此本函数必须只使用中断更新好的全局 shaft_angle，不能在任务里再调用 getAngle()/
// getVelocity() —— 两处并发读同一编码器（SPI 事务互相打断 + 多圈累加状态被竞争改写）
// 会让速度估算彻底失效（实测 V 在 -429~+308 之间乱跳，而真实转速稳定）。
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
