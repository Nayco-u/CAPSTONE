#ifndef BMS_H
#define BMS_H

#pragma once
#include <Arduino.h>
#include "utils.h"

// Estados del BMS
typedef enum {
    BMS_NORMAL,
    BMS_SOBREDESCARGA,
    BMS_SOBRECARGA
} BMSState;

// Inicialización del BMS (PWM ventilador, pines de protección, etc.)
void initBMS();

// Tarea de control del ventilador
void BMS_Fan_Task(void *pvParameters);

// (Más adelante) Tarea de protección y control de relé/transistor
void BMS_State_Task(void *pvParameters);

#endif