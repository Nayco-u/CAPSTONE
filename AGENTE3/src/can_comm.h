#pragma once
#include <Arduino.h>
#include "driver/twai.h"
#include "utils.h"

void initCAN(uint8_t txPin, uint8_t rxPin);
void startCANTasks(uint16_t agent_id);