#include "mppt.h"
#include "utils.h"

static int duty_cycle = 0;
static int duty_direction = 1;
static float previous_power = 0.0;

void initMPPT() {
    ledcSetup(MPPT_PWM_CHANNEL, MPPT_PWM_FREQ, MPPT_PWM_RESOLUTION);
    ledcAttachPin(MPPT_PWM_PIN, MPPT_PWM_CHANNEL);
    ledcWrite(MPPT_PWM_CHANNEL, duty_cycle);
}

void MPPT_Task(void *pvParameters) {
    const TickType_t intervalo = pdMS_TO_TICKS(MPPT_UPDATE_INTERVAL_MS);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true) {
        float voltage = 0.0, corriente = 0.0, power = 0.0;

        // Obtener mediciones del panel solar desde agentes_data
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            int16_t v_raw = agentes_data[local_agent_id].v_panel;
            int16_t i_raw = agentes_data[local_agent_id].i_panel;

            voltage = ads1.computeVolts(v_raw);
            corriente = ads2.computeVolts(i_raw) / MPPT_SENSIBILIDAD;
            xSemaphoreGive(dataMutex);
        }

        power = voltage * corriente;

        // Algoritmo P&O
        if (power > previous_power) {
            // mantener dirección
        } else {
            duty_direction = -duty_direction;
        }

        duty_cycle += duty_direction * MPPT_DUTY_STEP;
        duty_cycle = constrain(duty_cycle, MPPT_DUTY_MIN, MPPT_DUTY_MAX);
        ledcWrite(MPPT_PWM_CHANNEL, duty_cycle);

        previous_power = power;

        Serial.printf("[MPPT] V: %.2f V | I: %.2f A | P: %.2f W | Duty: %d\n", voltage, corriente, power, duty_cycle);
        vTaskDelayUntil(&lastWakeTime, intervalo);
    }
}