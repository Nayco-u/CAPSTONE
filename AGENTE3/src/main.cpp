#include <Arduino.h>
#include "utils.h"
#include "can_comm.h"
#include "pwm_control.h"
#include "sensors.h"
#include "firebase_task.h"
#include "bms.h"
#include "mppt.h"

/* void processSerialCommands(void *pvParameter) {
    while (true) {
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            // Formato: KP1=2.5, KI2=1.2, etc.
            if (cmd.startsWith("q")) { KP_1 = cmd.substring(1).toFloat(); integral_error_v = 0.0; integral_error_i = 0.0; }
            else if (cmd.startsWith("w")) { KI_1 = cmd.substring(1).toFloat(); integral_error_v = 0.0; integral_error_i = 0.0; }
            else if (cmd.startsWith("e")) { KP_2 = cmd.substring(1).toFloat(); integral_error_v = 0.0; integral_error_i = 0.0; }
            else if (cmd.startsWith("r")) { KI_2 = cmd.substring(1).toFloat(); integral_error_v = 0.0; integral_error_i = 0.0; }
            else if (cmd.startsWith("t")) { KP_3 = cmd.substring(1).toFloat(); integral_error_soc = 0.0; }
            else if (cmd.startsWith("y")) { KI_3 = cmd.substring(1).toFloat(); integral_error_soc = 0.0; }

            Serial.print("KP_1: "); Serial.print(KP_1);
            Serial.print(" | KI_1: "); Serial.print(KI_1);
            Serial.print(" | KP_2: "); Serial.print(KP_2);
            Serial.print(" | KI_2: "); Serial.print(KI_2);
            Serial.print(" | KP_3: "); Serial.print(KP_3);
            Serial.print(" | KI_3: "); Serial.println(KI_3);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Espera 50 ms antes de revisar de nuevo
    }
} */

TaskHandle_t handlePWM = NULL;

void setup() {
    Serial.begin(115200);

    // Configurar pines de LEDs
    setupStatusLED();

    // Inicializar WiFi y Firebase
    initWiFi();
    initFirebase();

    delay(1000);

    // Inicializar sensores
    initADS();
    Serial.println("Sensores inicializados.");

    // Inicializar PWM
    setupPWM();

    // Inicializar CAN
    initCAN(CAN_TX_PIN, CAN_RX_PIN);
    Serial.println("CAN inicializado.");

    // Inicializar BMS (ventilador, protecciones, etc.)
    initBMS();
    Serial.println("BMS inicializado.");

    // Inicializar MPPT
    initMPPT();
    Serial.println("MPPT inicializado.");

    // Crear Mutex global (solo una vez)
    dataMutex = xSemaphoreCreateMutex();

    // Iniciar tareas CAN
    startCANTasks(local_agent_id);

    // Tarea Firebase (puedes usar local_agent_id como número de agente)
    xTaskCreatePinnedToCore(Firebase_Update_Task, "Firebase_Update", 8192, NULL, 1, NULL, 0);

    // Crear tarea de control PWM
    xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, &handlePWM, 1);

    // Crear tarea para el ventilador del BMS
    xTaskCreatePinnedToCore(BMS_Fan_Task, "BMS_Fan", 2048, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(BMS_State_Task, "BMS_State", 2048, NULL, 1, NULL, 1);

    // Crear tarea para el MPPT
    xTaskCreatePinnedToCore(MPPT_Task, "MPPT_Task", 4096, NULL, 1, NULL, 1);

}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(50)); // No bloquea FreeRTOS, solo revisa cada 50 ms
}