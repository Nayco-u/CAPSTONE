#ifndef MPPT_H
#define MPPT_H

#include <Arduino.h>
#include <utils.h>

void initMPPT();                    // Inicialización PWM y sensores
void MPPT_Task(void *pvParameters); // Tarea de control MPPT

#endif
