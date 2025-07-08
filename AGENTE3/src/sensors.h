#ifndef SENSORS_H
#define SENSORS_H

#pragma once
#include <Arduino.h>
#include "utils.h"

void initADS();
void calibrarBias();
void leerSensores();

#endif