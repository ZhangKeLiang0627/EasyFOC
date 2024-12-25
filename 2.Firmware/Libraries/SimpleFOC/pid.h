#ifndef __PID_H
#define __PID_H

/******************************************************************************/
typedef struct
{
    float P;                      //!< Proportional gain
    float I;                      //!< Integral gain
    float D;                      //!< Derivative gain
    float output_ramp;            //!< Maximum speed of change of the output value
    float limit;                  //!< Maximum output value
    float error_prev;             //!< last tracking error value
    float output_prev;            //!< last pid output value
    float integral_prev;          //!< last integral component value
    unsigned long timestamp_prev; //!< Last execution timestamp
} PIDController;

extern PIDController PID_current_q, PID_current_d, PID_velocity, P_angle;
/******************************************************************************/
void PID_init(void);
float PIDoperator(PIDController *PID, float error);
/******************************************************************************/

#endif
