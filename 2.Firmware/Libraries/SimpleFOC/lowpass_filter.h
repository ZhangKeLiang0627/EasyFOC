#ifndef __LOWPASS_FILTER_H
#define __LOWPASS_FILTER_H

/******************************************************************************/
typedef struct
{
	float Tf;					  //!< Low pass filter time constant
	float y_prev;				  //!< filtered value in previous execution step
	unsigned long timestamp_prev; //!< Last execution timestamp
} LowPassFilter;

extern LowPassFilter LPF_current_q, LPF_current_d, LPF_velocity;
/******************************************************************************/
void LPF_init(void);
float LPFoperator(LowPassFilter *LPF, float x);
/******************************************************************************/

#endif
