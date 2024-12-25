
#include "FOCuser_Inc.h"

/******************************************************************************/
LowPassFilter LPF_current_q, LPF_current_d, LPF_velocity;
/******************************************************************************/
void LPF_init(void)
{
	LPF_current_q.Tf = 0.02f;
	LPF_current_q.y_prev = 0;
	LPF_current_q.timestamp_prev = _micros();

	LPF_current_d.Tf = 0.02f;
	LPF_current_d.y_prev = 0;
	LPF_current_d.timestamp_prev = _micros();

	LPF_velocity.Tf = 0.02f;
	LPF_velocity.y_prev = 0;
	LPF_velocity.timestamp_prev = _micros();
}
/******************************************************************************/
float LPFoperator(LowPassFilter *LPF, float x)
{
	unsigned long timestamp;
	float dt, alpha, y;

	timestamp = _micros();
	dt = (timestamp - LPF->timestamp_prev) * 1e-6f;
	LPF->timestamp_prev = timestamp;
	if (dt < 0.0f)
		dt = 1e-3f;
	else if (dt > 0.3f)
	{
		LPF->y_prev = x;
		return x;
	}

	alpha = LPF->Tf / (LPF->Tf + dt);
	y = alpha * LPF->y_prev + (1.0f - alpha) * x;
	LPF->y_prev = y;

	return y;
}
/******************************************************************************/
