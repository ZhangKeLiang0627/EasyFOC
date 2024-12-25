
#include "FOCuser_Inc.h"

/* InlineCurrentSense 内置电流采样法 */

/******************************************************************************/
int pinA, pinB, pinC;
float gain_a, gain_b, gain_c;
float offset_ia, offset_ib, offset_ic;
/******************************************************************************/
void Current_calibrateOffsets(void);
/******************************************************************************/

// 注意，这里移除了关于pinC的电流采样ADC初始化，pinC只能填NOT_SET
void InlineCurrentSense(float _shunt_resistor, float _gain, int _pinA, int _pinB, int _pinC)
{
	float volts_to_amps_ratio;

	pinA = _pinA;
	pinB = _pinB;
	pinC = _pinC;

	volts_to_amps_ratio = 1.0f / _shunt_resistor / _gain; // volts to amps

	gain_a = volts_to_amps_ratio;
	// gain_b = -volts_to_amps_ratio;
	gain_b = volts_to_amps_ratio;
	gain_c = volts_to_amps_ratio;

	printf("gain_a:%.2f, gain_b:%.2f, gain_c:%.2f.\r\n", gain_a, gain_b, gain_c);
}
/******************************************************************************/
void InlineCurrentSense_Init(void)
{
	// EasyFOC硬件上去除了pinC的电流采样，于是初始化函数改为MyADC_Init();
	// configureADCInline(pinA, pinB, pinC);

	// MyADC_Init(); // 已经在主函数中进行初始化

	Current_calibrateOffsets(); // 检测偏置电压，也就是电流0A时的运放输出电压值，理论值 = 1.65V
}
/******************************************************************************/
// Function finding zero offsets of the ADC
void Current_calibrateOffsets(void)
{
	int i;

	offset_ia = 0;
	offset_ib = 0;
	offset_ic = 0;
	// read the adc voltage 1000 times ( arbitrary number )
	for (i = 0; i < 1000; i++)
	{
		offset_ia += _readADCVoltageInline(pinA);
		offset_ib += _readADCVoltageInline(pinB);
		if (_isset(pinC))
			offset_ic += _readADCVoltageInline(pinC);
		delay_ms(1);
	}
	// calculate the mean offsets
	offset_ia = offset_ia / 1000;
	offset_ib = offset_ib / 1000;
	if (_isset(pinC))
		offset_ic = offset_ic / 1000;

	printf("offset_ia:%.4f, offset_ib:%.4f, offset_ic:%.4f.\r\n", offset_ia, offset_ib, offset_ic);
}
/******************************************************************************/
// read all three phase currents (if possible 2 or 3)
PhaseCurrent_s getPhaseCurrents(void)
{
	PhaseCurrent_s current;

	current.a = (_readADCVoltageInline(pinA) - offset_ia) * gain_a;						  // amps
	current.b = (_readADCVoltageInline(pinB) - offset_ib) * gain_b;						  // amps
	current.c = (!_isset(pinC)) ? 0 : (_readADCVoltageInline(pinC) - offset_ic) * gain_c; // amps

	return current;
}
/******************************************************************************/
