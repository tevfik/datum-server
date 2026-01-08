#ifndef FIXED_PID_CONTROLLER_H
#define FIXED_PID_CONTROLLER_H

#include "fix16.h"

typedef struct {

	// Controller gains 
	fix16_t Kp;
	fix16_t Ki;
	fix16_t Kd;

	// Derivative low-pass filter time constant
	fix16_t tau;

	// Output limits
	fix16_t limMin;
	fix16_t limMax;
	
	// Integrator limits
	fix16_t limMinInt;
	fix16_t limMaxInt;

	// Sample time (in seconds)
	fix16_t interval;
    fix16_t setpoint;
    fix16_t measurement;

	// Controller "memory"
    fix16_t error;
    fix16_t proportional;
	fix16_t integrator;
	fix16_t differentiator;
	fix16_t prevError;              // Required for integrator   
	fix16_t prevMeasurement;		// Required for differentiator 

	// Controller output 
	fix16_t out;

} PIDController;

void  PIDController_Init(PIDController *pid);
fix16_t PIDController_Update(PIDController *pid, fix16_t setpoint, fix16_t measurement, fix16_t interval);

#endif
