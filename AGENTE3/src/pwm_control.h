#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#pragma once
#include <Arduino.h>
#include "utils.h"
#include "sensors.h"

// Prototipos
void setupPWM();
void PWM_Control_Task(void *pvParameters);

#endif
