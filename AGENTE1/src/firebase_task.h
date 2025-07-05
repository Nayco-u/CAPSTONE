#ifndef FIREBASE_COMM_H
#define FIREBASE_COMM_H

#pragma once
#include <Arduino.h>
#include "utils.h"

void Firebase_Update_Task(void *parameter);
void initWiFi();
void initFirebase();
void databaseResult(AsyncResult &aResult);

#endif