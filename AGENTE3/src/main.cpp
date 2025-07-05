#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include "utils.h"
#include "can_comm.h"
#include "pwm_control.h"
#include "sensors.h"
#include "firebase_task.h"
#include "bms.h"

void setup() {
    Serial.begin(115200);

    // Inicializar WiFi y Firebase
    initWiFi();
    initFirebase();

    // Inicializar sensores
    initADS();
    calibrarBias();

    // Inicializar PWM
    setupPWM();

    // Inicializar CAN
    initCAN(CAN_TX_PIN, CAN_RX_PIN);

    // Inicializar BMS (ventilador, protecciones, etc.)
    initBMS();

    // Crear Mutex global (solo una vez)
    dataMutex = xSemaphoreCreateMutex();

    // Crear tareas y guardar los handles
    xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, NULL, 1);

    // Iniciar tareas CAN
    startCANTasks(local_agent_id);

    // Tarea Firebase (puedes usar local_agent_id como número de agente)
    xTaskCreatePinnedToCore(Firebase_Update_Task, "Firebase_Update", 8192, NULL, 1, NULL, 0);

    // Crear tarea para el ventilador del BMS
    xTaskCreatePinnedToCore(BMS_Fan_Task, "BMS_Fan", 2048, NULL, 1, NULL, 1);
}

void loop() {
    // El loop principal queda vacío, ya que las tareas manejan la lógica
}