
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
// 速度差分窗口（个采样点，@1kHz 任务 = 5ms）。低速时 1ms 差分只转过 2-3 个编码器 LSB，
// 速度被量化成 0.384rad/s 台阶致顿挫；窗口拉到 5ms 台阶降至约 0.077rad/s。
#define VEL_WINDOW 5

// shaft velocity calculation
// 滑动窗口差分：窗口 VEL_WINDOW 个采样点，每次调用（1kHz）都更新速度，速度环频率不变
float shaftVelocity(void)
{
  static float ang_hist[VEL_WINDOW];
  static uint32_t ts_hist[VEL_WINDOW];
  static uint8_t hist_cnt = 0;
  uint32_t now_us = _micros();
  float Ts, vel;
  uint8_t i;

  // 右移历史，插入当前快照
  for (i = VEL_WINDOW - 1; i > 0; i--)
  {
    ang_hist[i] = ang_hist[i - 1];
    ts_hist[i] = ts_hist[i - 1];
  }
  ang_hist[0] = shaft_angle;
  ts_hist[0] = now_us;
  if (hist_cnt < VEL_WINDOW)
    hist_cnt++;

  if (hist_cnt < 2)
    return 0;

  Ts = (ts_hist[0] - ts_hist[hist_cnt - 1]) * 1e-6f;
  if (Ts <= 0 || Ts > 0.5f)
    Ts = 1e-3f;

  vel = (ang_hist[0] - ang_hist[hist_cnt - 1]) / Ts;

  return LPFoperator(&LPF_velocity, vel);
}
/******************************************************************************/
float electricalAngle(void)
{
  return _normalizeAngle((shaft_angle + sensor_offset) * pole_pairs - zero_electric_angle);
}
/******************************************************************************/
