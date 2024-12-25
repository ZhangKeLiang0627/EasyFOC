#include "MagneticSensor.h"
#include "AS5600.h"
#include "AS5047P.h"
#include <math.h>

#define abs(x) ((x) > 0 ? (x) : -(x))
#define _2PI 6.28318530718f

uint16_t (*_GetRawAngle)(void); // 获取初始角度数据的函数指针

unsigned long velocity_calc_timestamp; // 速度计时，用于计算速度
float angle_prev;                      // 获取速度用
static float angle_data_prev = 0;      // 获取角度用
static float full_rotation_offset;     // 角度累加
long cpr = AS5600_CPR;                 // 编码器采样精度（如ADC12bit / ADC14bit）

static uint8_t SensorOption = MAGNETIC_SENSOR_AS5600; // 磁传感器选择

/**
 * @brief MagneticSensor_Init
 * @brief 初始化磁传感器
 * @param  无
 * @retval 0（成功），1（失败）
 */
uint8_t MagneticSensor_Init(void)
{
    switch (SensorOption)
    {
    case MAGNETIC_SENSOR_AS5600:
        /* init Sensor Drive */
        AS5600_Init();

        /* init function */
        _GetRawAngle = AS5600_GetRawAngle;

        /* init param */
        cpr = AS5600_CPR;

        angle_data_prev = AS5600_GetRawAngle();
        full_rotation_offset = 0;
        velocity_calc_timestamp = _micros(); // 获得当前时间帧
        angle_prev = getAngle();
        break;

    case MAGNETIC_SENSOR_AS5047P:
        /* init Sensor Drive */
        AS5047P_Init();

        /* init function */
        _GetRawAngle = AS5047P_GetRawAngle;

        /* init param */
        cpr = AS5047P_CPR;

        angle_data_prev = AS5047P_GetRawAngle();
        full_rotation_offset = 0;
        velocity_calc_timestamp = _micros(); // 获得当前时间帧
        angle_prev = getAngle();
        break;

    default:
        printf("[MagneticSensor] Did not choose correct sensor!\r\n");
        return 1;
    }

    printf("[MagneticSensor] MagneticSensor init success!\r\n");
    return 0;
}

/**
 * @brief MagneticSensor_OptionSelect
 * @brief 选择磁传感器
 * @param  option
 * @retval 0（成功），1（失败）
 */
uint8_t MagneticSensor_OptionSelect(uint8_t option)
{
    switch (option)
    {
    case MAGNETIC_SENSOR_AS5600:
        SensorOption = MAGNETIC_SENSOR_AS5600;
        break;

    case MAGNETIC_SENSOR_AS5047P:
        SensorOption = MAGNETIC_SENSOR_AS5047P;
        break;

    default:
        printf("[MagneticSensor] Did not select correct sensor!\r\n");
        return 1;
    }

    printf("[MagneticSensor] MagneticSensor select success!\r\n");
    return 0;
}

float getAngle(void)
{
    float d_angle;

    // float angle_data = AS5600_GetRawAngle();
    // float angle_data = AS5047P_GetRawAngle();
    float angle_data = _GetRawAngle();

    // tracking the number of rotations
    // in order to expand angle range form [0,2PI] to basically infinity
    d_angle = angle_data - angle_data_prev;
    // if overflow happened track it as full rotation
    if (fabs(d_angle) > (0.8f * cpr))
        full_rotation_offset += d_angle > 0 ? -_2PI : _2PI;
    // save the current angle value for the next steps
    // in order to know if overflow happened
    angle_data_prev = angle_data;
    // return the full angle
    // (number of full rotations)*2PI + current sensor angle
    return (full_rotation_offset + (angle_data / (float)cpr) * _2PI);
}

// float getAngle(void)
// {
//     long angle_data, d_angle;

//     angle_data = _GetRawAngle();

//     // tracking the number of rotations
//     // in order to expand angle range form [0,2PI] to basically infinity
//     d_angle = angle_data - angle_data_prev;
//     // if overflow happened track it as full rotation
//     if (abs(d_angle) > (0.8f * cpr))
//         full_rotation_offset += (d_angle > 0) ? -_2PI : _2PI;
//     // save the current angle value for the next steps
//     // in order to know if overflow happened
//     angle_data_prev = angle_data;

//     if (full_rotation_offset >= (_2PI * 2000)) // 转动圈数过多后浮点数精度下降，电流增加并最终堵转，每隔一定圈数归零一次
//     {                                          // 这个问题针对电机长时间连续一个方向转动
//         full_rotation_offset = 0;              // 速度模式，高速转动时每次归零会导致电机抖动一次
//         angle_prev = angle_prev - _2PI * 2000;
//     }
//     if (full_rotation_offset <= (-_2PI * 2000))
//     {
//         full_rotation_offset = 0;
//         angle_prev = angle_prev + _2PI * 2000;
//     }

//     // return the full angle
//     // (number of full rotations)*2PI + current sensor angle
//     return (full_rotation_offset + ((float)angle_data / cpr * _2PI));
// }

// Shaft velocity calculation
float getVelocity(void)
{
    long now_us;
    float Ts, angle_now, vel;

    // calculate sample time
    now_us = _micros();
    Ts = (now_us - velocity_calc_timestamp) * 1e-6f;
    // quick fix for strange cases (micros overflow)
    if (Ts <= 0 || Ts > 0.5f)
        Ts = 1e-3f;

    // current angle
    angle_now = getAngle();
    // velocity calculation
    vel = (angle_now - angle_prev) / Ts;

    // save variables for future pass
    angle_prev = angle_now;
    velocity_calc_timestamp = now_us;
    return vel;
}
