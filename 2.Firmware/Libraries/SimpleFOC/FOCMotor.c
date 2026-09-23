
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
float shaftVelocity(void)
{
  // 注意：改为基于「20kHz 中断发布的 volatile shaft_angle」做差分，
  // 不再调用 getVelocity()/getAngle() —— 否则会与中断里的 getAngle() 争抢
  // getAngle() 内部的 static 多圈累加状态，导致数据竞态。
  // shaft_angle = sensor_direction * getAngle() - sensor_offset，本身已带方向，
  // 差分结果即为带方向的机械角速度，无需再乘 sensor_direction。
  static float angle_prev = 0;
  static uint32_t timestamp_prev = 0;
  static uint8_t first = 1;
  uint32_t now_us = _micros();
  float Ts, vel;

  if (first)
  {
    angle_prev = shaft_angle;
    timestamp_prev = now_us;
    first = 0;
    return 0; // 首次调用仅初始化，返回 0
  }

  Ts = (now_us - timestamp_prev) * 1e-6f;
  if (Ts <= 0 || Ts > 0.5f)
    Ts = 1e-3f;

  vel = (shaft_angle - angle_prev) / Ts;

  angle_prev = shaft_angle;
  timestamp_prev = now_us;

  return LPFoperator(&LPF_velocity, vel);
}
/******************************************************************************/
float electricalAngle(void)
{
  return _normalizeAngle((shaft_angle + sensor_offset) * pole_pairs - zero_electric_angle);
}
/******************************************************************************/
