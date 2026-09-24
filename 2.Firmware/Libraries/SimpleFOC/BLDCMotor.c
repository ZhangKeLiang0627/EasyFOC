
#include "FOCuser_Inc.h"

/******************************************************************************/
extern float target;	 // main.c
extern float angle_prev; // as5600.c
/******************************************************************************/
long sensor_direction;
float voltage_sensor_align;
float voltage_power_supply;
float voltage_limit;
int pole_pairs;
unsigned long open_loop_timestamp;
float velocity_limit;
float current_limit;
/******************************************************************************/
// DWT 计时统计（loopFOCISR 单次执行耗时，单位 CPU 周期 @84MHz）
uint32_t foc_cycles_min = 0xFFFFFFFF;
uint32_t foc_cycles_max = 0;
uint64_t foc_cycles_sum = 0; // uint64：20kHz 下约 152s 就会让 uint32 溢出回绕，导致 avg 显示假数据
uint32_t foc_cycles_cnt = 0;
/******************************************************************************/
int alignSensor(void);
float velocityOpenloop(float target_velocity);
float angleOpenloop(float target_angle);
/******************************************************************************/
void Motor_init(void)
{
	printf("MOT: Init\r\n");

	//	new_voltage_limit = current_limit * phase_resistance;
	//	voltage_limit = new_voltage_limit < voltage_limit ? new_voltage_limit : voltage_limit;
	if (voltage_sensor_align > voltage_limit)
		voltage_sensor_align = voltage_limit;

	// current control loop controls voltage
	PID_current_q.limit = voltage_limit;
	PID_current_d.limit = voltage_limit;
	// velocity control loop controls current
	// 如果是电压模式限制电压，如果是电流模式限制电流
	if (torque_controller == Type_voltage)
		PID_velocity.limit = voltage_limit; // 速度模式的电流限制
	else
		PID_velocity.limit = current_limit;
	P_angle.limit = velocity_limit; // 角度模式的速度限制

	M1_Enable();
	printf("MOT: Enable driver.\r\n");
}
/******************************************************************************/
void Motor_initFOC(float zero_electric_offset, Direction _sensor_direction)
{
	if (zero_electric_offset != 0 && _sensor_direction != UNKNOWN)
	{
	// abosolute zero offset provided - no need to align
	zero_electric_angle = zero_electric_offset;
	// set the sensor direction - default CW
 	sensor_direction = _sensor_direction;
	}

	alignSensor(); // 检测零点偏移量和极对数

	// added the shaft_angle update
	angle_prev = getAngle(); // getVelocity(), make sure velocity = 0 after power on
	delay_ms(50);
	shaft_angle = shaftAngle(); // 先更新 shaft_angle，供 shaftVelocity() 差分初始化（避免首次算出虚假速度）
	shaft_velocity = shaftVelocity(); // 必须调用一次，进入主循环后速度为0
	delay_ms(5);
	if (controller == Type_angle)
		target = shaft_angle; // 角度模式，以当前的角速度为目标角度，进入主循环后电机静止

	delay_ms(200);
}
/******************************************************************************/
int alignSensor(void)
{
	long i;
	float angle;
	float mid_angle, end_angle;
	float moved;

	printf("MOT: Align sensor.\r\n");

	if (sensor_direction == UNKNOWN) // 没有设置,需要检测
	{
		// find natural direction
		// move one electrical revolution forward
		for (i = 0; i <= 500; i++)
		{
			angle = _3PI_2 + _2PI * i / 500.0f;
			setPhaseVoltage(voltage_sensor_align, 0, angle);
			delay_ms(2);
		}
		mid_angle = getAngle();

		for (i = 500; i >= 0; i--)
		{
			angle = _3PI_2 + _2PI * i / 500.0f;
			setPhaseVoltage(voltage_sensor_align, 0, angle);
			delay_ms(2);
		}
		end_angle = getAngle();
		setPhaseVoltage(0, 0, 0);
		delay_ms(200);

		printf("mid_angle=%.4f\r\n", mid_angle);
		printf("end_angle=%.4f\r\n", end_angle);

		moved = fabs(mid_angle - end_angle);
		if ((mid_angle == end_angle) || (moved < 0.01f)) // 相等或者几乎没有动
		{
			printf("MOT: Failed to notice movement.\r\n");
			M1_Disable(); // 电机检测不正常,关闭驱动
			return 0;
		}
		else if (mid_angle < end_angle)
		// else if (mid_angle > end_angle)
		{
			printf("MOT: sensor_direction == CCW\r\n");
			sensor_direction = CCW;
		}
		else
		{
			printf("MOT: sensor_direction == CW\r\n");
			sensor_direction = CW;
		}

		printf("MOT: PP check: ");					// 计算Pole_Pairs
		if (fabs(moved * pole_pairs - _2PI) > 0.5f) // 0.5 is arbitrary number it can be lower or higher!
		{
			printf("fail - estimated pp:");
			pole_pairs = _2PI / moved + 0.5f; // 浮点数转整型,四舍五入
			printf("pole_pairs = %d\r\n", pole_pairs);
		}
		else
			printf("OK!\r\n");
	}
	else
		printf("MOT: Skip dir calib.\r\n");

	if (zero_electric_angle == 0) // 没有设置,需要检测
	{
		setPhaseVoltage(voltage_sensor_align, 0, _3PI_2); // 计算零点偏移角度
		delay_ms(700);
		zero_electric_angle = _normalizeAngle(_electricalAngle(sensor_direction * getAngle(), pole_pairs));
		delay_ms(20);
		printf("MOT: Zero elec. angle:");
		printf("%.4f\r\n", zero_electric_angle);
		setPhaseVoltage(0, 0, 0);
		delay_ms(200);
	}
	else
		printf("MOT: Skip offset calib.\r\n");

	return 1;
}
/******************************************************************************/
void loopFOC(void)
{
	if (controller == Type_angle_openloop || controller == Type_velocity_openloop)
		return;

	shaft_angle = shaftAngle();			  // shaft angle
	electrical_angle = electricalAngle(); // electrical angle - need shaftAngle to be called first

	switch (torque_controller)
	{
	case Type_voltage: // no need to do anything really
		break;
	case Type_dc_current:
		// read overall current magnitude
		current.q = getDCCurrent(electrical_angle);
		// filter the value values
		current.q = LPFoperator(&LPF_current_q, current.q);
		// calculate the phase voltage
		voltage.q = PIDoperator(&PID_current_q, (current_sp - current.q));
		voltage.d = 0;
		break;
	case Type_foc_current:
		// read dq currents
		current = getFOCCurrents(electrical_angle);
		// filter values
		current.q = LPFoperator(&LPF_current_q, current.q);
		current.d = LPFoperator(&LPF_current_d, current.d);
		// calculate the phase voltages
		voltage.q = PIDoperator(&PID_current_q, (current_sp - current.q));
		voltage.d = PIDoperator(&PID_current_d, -current.d);
		break;
	default:
		printf("MOT: no torque control selected!\r\n");
		break;
	}
	// set the phase voltage - FOC heart function :)
	setPhaseVoltage(voltage.q, voltage.d, electrical_angle);
}
/******************************************************************************/
// 20kHz 中断版电流环（TIM10 更新中断里调用）
// 与原 loopFOC() 的差异：
//   1. 采样改用规则组 analogRead（注入组 JDR2 有硬件坑已弃用，见 getPhaseCurrentsISR 注释）；
//   2. PID/LPF 用固定 dt 版本（FOC_ISR_TS = 50us），不读 SysTick（高优先级中断里 SysTick 被挂起）；
//   3. 去掉 printf（中断里禁止），default 分支静默。
void loopFOCISR(void)
{
	uint32_t t0, t1, dt;

	t0 = DWT_GetCycle(); // 计时起点

	if (controller == Type_angle_openloop || controller == Type_velocity_openloop)
		return;

	// 读角度（SPI 阻塞约 2-3us）
	shaft_angle = shaftAngle();			  // shaft angle
	electrical_angle = electricalAngle(); // electrical angle - need shaftAngle to be called first

	switch (torque_controller)
	{
	case Type_voltage: // no need to do anything really
		break;
	case Type_dc_current:
		// read overall current magnitude（规则组 analogRead 采样）
		current.q = getDCCurrentISR(electrical_angle);
		// filter the value values
		current.q = LPFoperator_dt(&LPF_current_q, current.q, FOC_ISR_TS);
		// calculate the phase voltage
		voltage.q = PIDoperator_dt(&PID_current_q, (current_sp - current.q), FOC_ISR_TS);
		voltage.d = 0;
		break;
	case Type_foc_current:
		// read dq currents（规则组 analogRead 采样）
		current = getFOCCurrentsISR(electrical_angle);
		// filter values
		current.q = LPFoperator_dt(&LPF_current_q, current.q, FOC_ISR_TS);
		current.d = LPFoperator_dt(&LPF_current_d, current.d, FOC_ISR_TS);
		// calculate the phase voltages
		voltage.q = PIDoperator_dt(&PID_current_q, (current_sp - current.q), FOC_ISR_TS);
		voltage.d = PIDoperator_dt(&PID_current_d, -current.d, FOC_ISR_TS);
		break;
	default:
		// 中断里禁止 printf，静默处理
		break;
	}
	// set the phase voltage - FOC heart function :)
	setPhaseVoltage(voltage.q, voltage.d, electrical_angle);

	// 计时统计
	t1 = DWT_GetCycle();
	dt = t1 - t0;
	if (dt < foc_cycles_min)
		foc_cycles_min = dt;
	if (dt > foc_cycles_max)
		foc_cycles_max = dt;
	foc_cycles_sum += dt;
	foc_cycles_cnt++;
}
/******************************************************************************/
void move(float new_target)
{
	shaft_velocity = shaftVelocity();

	switch (controller)
	{
	case Type_torque:
		if (torque_controller == Type_voltage)
		{
			voltage.q = new_target; // if voltage torque control
		}
		else
			current_sp = _constrain(new_target, -current_limit, current_limit); // 电流目标限幅，防止大电流命令失控
		break;
	case Type_angle:
		// angle set point
		shaft_angle_sp = new_target;
		// calculate velocity set point
		shaft_velocity_sp = PIDoperator(&P_angle, (shaft_angle_sp - shaft_angle));
		// calculate the torque command
		current_sp = PIDoperator(&PID_velocity, (shaft_velocity_sp - shaft_velocity)); // if voltage torque control
		// if torque controlled through voltage
		if (torque_controller == Type_voltage)
		{
			voltage.q = current_sp;
			voltage.d = 0;
		}
		break;
	case Type_velocity:
		// velocity set point
		shaft_velocity_sp = new_target;
		// calculate the torque command
		current_sp = PIDoperator(&PID_velocity, (shaft_velocity_sp - shaft_velocity)); // if current/foc_current torque control
		// if torque controlled through voltage control
		if (torque_controller == Type_voltage)
		{
			voltage.q = current_sp; // use voltage if phase-resistance not provided
			voltage.d = 0;
		}
		break;
	case Type_velocity_openloop:
		// velocity control in open loop
		shaft_velocity_sp = new_target;
		voltage.q = velocityOpenloop(shaft_velocity_sp); // returns the voltage that is set to the motor
		voltage.d = 0;
		break;
	case Type_angle_openloop:
		// angle control in open loop
		shaft_angle_sp = new_target;
		voltage.q = angleOpenloop(shaft_angle_sp); // returns the voltage that is set to the motor
		voltage.d = 0;
		break;
	}
}
/******************************************************************************/
void setPhaseVoltage(float Uq, float Ud, float angle_el)
{
	float Uout;
	uint32_t sector;
	float T0, T1, T2;
	float Ta, Tb, Tc;

	if (Ud) // only if Ud and Uq set
	{		// _sqrt is an approx of sqrt (3-4% error)
		Uout = _sqrt(Ud * Ud + Uq * Uq) / voltage_power_supply;
		// angle normalisation in between 0 and 2pi
		// only necessary if using _sin and _cos - approximation functions
		angle_el = _normalizeAngle(angle_el + atan2(Uq, Ud));
	}
	else
	{ // only Uq available - no need for atan2 and sqrt
		Uout = Uq / voltage_power_supply;
		// angle normalisation in between 0 and 2pi
		// only necessary if using _sin and _cos - approximation functions
		angle_el = _normalizeAngle(angle_el + _PI_2);
	}

	// if (Uout > 0.577f)
	// 	Uout = 0.577f;
	// if (Uout < -0.577f)
	// 	Uout = -0.577f;

	sector = (angle_el / _PI_3) + 1;
	T1 = _SQRT3 * _sin(sector * _PI_3 - angle_el) * Uout;
	T2 = _SQRT3 * _sin(angle_el - (sector - 1.0f) * _PI_3) * Uout;
	T0 = 1 - T1 - T2;

	// calculate the duty cycles(times)
	switch (sector)
	{
	case 1:
		Ta = T1 + T2 + T0 / 2;
		Tb = T2 + T0 / 2;
		Tc = T0 / 2;
		break;
	case 2:
		Ta = T1 + T0 / 2;
		Tb = T1 + T2 + T0 / 2;
		Tc = T0 / 2;
		break;
	case 3:
		Ta = T0 / 2;
		Tb = T1 + T2 + T0 / 2;
		Tc = T2 + T0 / 2;
		break;
	case 4:
		Ta = T0 / 2;
		Tb = T1 + T0 / 2;
		Tc = T1 + T2 + T0 / 2;
		break;
	case 5:
		Ta = T2 + T0 / 2;
		Tb = T0 / 2;
		Tc = T1 + T2 + T0 / 2;
		break;
	case 6:
		Ta = T1 + T2 + T0 / 2;
		Tb = T0 / 2;
		Tc = T1 + T0 / 2;
		break;
	default: // possible error state
		Ta = 0;
		Tb = 0;
		Tc = 0;
	}

	TIM_SetCompare2(M1_TIMx, Ta * PWM_Period);
	TIM_SetCompare3(M1_TIMx, Tb * PWM_Period);
	TIM_SetCompare4(M1_TIMx, Tc * PWM_Period);
}
/******************************************************************************/
float velocityOpenloop(float target_velocity)
{
	unsigned long now_us;
	float Ts, Uq;

	now_us = _micros();							 // get current timestamp
	Ts = (now_us - open_loop_timestamp) * 1e-6f; // calculate the sample time from last call
	if (Ts <= 0 || Ts > 0.5f)
		Ts = 1e-3f;				  // quick fix for strange cases (micros overflow + timestamp not defined)
	open_loop_timestamp = now_us; // save timestamp for next call

	// calculate the necessary angle to achieve target velocity
	shaft_angle = _normalizeAngle(shaft_angle + target_velocity * Ts);
	// for display purposes
	shaft_velocity = target_velocity;

	Uq = voltage_sensor_align;
	// set the maximal allowed voltage (voltage_limit) with the necessary angle
	setPhaseVoltage(Uq, 0, _electricalAngle(shaft_angle, pole_pairs));

	return Uq;
}
/******************************************************************************/
float angleOpenloop(float target_angle)
{
	unsigned long now_us;
	float Ts, Uq;

	now_us = _micros(); // micros()*1000; //_micros();
	if (now_us < open_loop_timestamp)
		Ts = (float)(open_loop_timestamp - now_us) * 1e-6f;
	else
		Ts = (float)(0xFFFFFF - now_us + open_loop_timestamp) * 1e-6f;
	open_loop_timestamp = now_us; // save timestamp for next call
	// quick fix for strange cases (micros overflow)
	if (Ts == 0 || Ts > 0.5f)
		Ts = 1e-3f;

	// calculate the necessary angle to move from current position towards target angle
	// with maximal velocity (velocity_limit)
	if (fabs(target_angle - shaft_angle) > velocity_limit * Ts)
	{
		shaft_angle += _sign(target_angle - shaft_angle) * velocity_limit * Ts;
		// shaft_velocity = velocity_limit;
	}
	else
	{
		shaft_angle = target_angle;
		// shaft_velocity = 0;
	}

	Uq = voltage_limit;
	// set the maximal allowed voltage (voltage_limit) with the necessary angle
	setPhaseVoltage(Uq, 0, _electricalAngle(shaft_angle, pole_pairs));

	return Uq;
}
/******************************************************************************/
