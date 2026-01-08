#include "fix16pid.h"

void PIDController_Init(PIDController *pid) {

	// Clear controller variables
	pid->integrator = fix16_zero;
	pid->prevError  = fix16_zero;

	pid->differentiator  = fix16_zero;
	pid->prevMeasurement = fix16_zero;

	pid->out = fix16_zero;

}

fix16_t PIDController_Update(PIDController *pid, fix16_t setpoint, fix16_t measurement, fix16_t interval) {

    if (interval == fix16_zero)
        interval = pid->interval;
    
    pid->setpoint = setpoint;
    pid->measurement = measurement;
    
	// Error signal
    pid->error = fix16_ssub(setpoint, measurement);


	// Proportional
    pid->proportional = fix16_smul(pid->Kp , pid->error);


	// Integral
    pid->integrator =   fix16_sadd(
                            pid->integrator,
                            fix16_smul(
                                fix16_smul(fix16_half   , pid->Ki) , 
                                fix16_smul(interval     , (pid->error + pid->prevError))
                                      )
                        );

	// Anti-wind-up via integrator clamping 
    pid->integrator = fix16_clamp(pid->integrator,pid->limMinInt,pid->limMaxInt);

	// Derivative (band-limited differentiator)
    // Note: derivative on measurement, therefore minus sign in front of equation!
    if( pid->Kd == fix16_zero )
    {
        pid->differentiator = fix16_zero;
    }else{
        pid->differentiator = fix16_sdiv(
                                    fix16_sadd(
                                        fix16_smul( 
                                                fix16_smul(fix16_two,pid->Kd) , 
                                                fix16_ssub(measurement, pid->prevMeasurement) 
                                                ),
                                        fix16_smul(
                                                fix16_ssub(
                                                           fix16_smul(fix16_two,pid->tau), 
                                                           interval
                                                           ),
                                                pid->differentiator                           
                                                )
                                            ),
                                    fix16_sadd(
                                            fix16_smul(fix16_two,pid->tau),
                                            interval
                                            )
                                    );        
    }
	//Compute output and apply limits
    pid->out = fix16_sadd(
                        fix16_sadd(pid->proportional, pid->integrator), 
                        pid->differentiator
                        );
            
    pid->out = fix16_clamp(pid->out,pid->limMin,pid->limMax);

	// Store error and measurement for later use
    pid->prevError       = pid->error;
    pid->prevMeasurement = measurement;

	//  Return controller output
    return pid->out;

}
