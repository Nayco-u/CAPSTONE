#pragma once
#include <Arduino.h>
#include <driver/twai.h>
#include "utils.h"

// Inicialización del bus CAN
void initCAN(uint8_t txPin, uint8_t rxPin);

// Inicia las tareas CAN para el agente MAESTRO
void startCANMaster(uint16_t agent_id);

// Inicia las tareas CAN para el agente ESCLAVO
void startCANSlave(uint16_t agent_id);

// Tarea de monitoreo del bus CAN (opcional)
void CAN_Monitor_Task(void *pvParameters);
