#include "bms.h"

// Estado inicial del BMS
static BMSState bms_state = BMS_NORMAL;
extern TaskHandle_t handlePWM;

void initBMS() {
    // Configurar PWM del ventilador
    ledc_timer_config_t timerConfig = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .duty_resolution  = FAN_PWM_RES,
        .timer_num        = FAN_PWM_TIMER,
        .freq_hz          = FAN_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timerConfig);

    ledc_channel_config_t channelConfig = {
        .gpio_num   = FAN_PWM_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = FAN_PWM_CHANNEL,
        .timer_sel  = FAN_PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&channelConfig);

    // Inicializar pines del relé y transistor
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(TRANSISTOR_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);         // Relé cerrado (activo)
    digitalWrite(TRANSISTOR_PIN, HIGH);   // Transistor apagado
}

void BMS_Fan_Task(void *pvParameters) {
    while (true) {
        int16_t i_converter_local = 0;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            i_converter_local = agentes_data[local_agent_id].i_converter;
            xSemaphoreGive(dataMutex);
        }

        float i_A = fabs(i_converter_local * 0.0001875 * 10.0); // Conversión a amperios
        int duty = (int)(constrain(i_A, 0, 10.0) * 255.0 / 10.0); // 0-10A → 0-255

        ledc_set_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void BMS_State_Task(void *pvParameters) {
    while (true) {
        int16_t soc_local = 0;
        int16_t soc_prom = 0;
        int16_t i_converter_local = 0;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            soc_local = agentes_data[local_agent_id].soc;
            i_converter_local = agentes_data[local_agent_id].i_converter;

            int suma_soc = soc_local;
            int activos = 1;
            unsigned long now = millis();
            for (size_t i = 0; i < NUM_AGENTS; ++i) {
                uint8_t id = known_agents[i];
                if (now - agentes_data[id].last_update < 2000) {
                    suma_soc += agentes_data[id].soc;
                    activos++;
                }
            }
            soc_prom = suma_soc / activos;
            xSemaphoreGive(dataMutex);
        }

        switch (bms_state) {
            case BMS_NORMAL:
                if (soc_prom < SOC_MIN) {
                    // SOC promedio bajo: desconectar
                    digitalWrite(RELAY_PIN, HIGH);
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                    bms_state = BMS_SOBREDESCARGA;
                    Serial.println("[BMS] Estado: SOBREDESCARGA");
                } else if (soc_local > SOC_MAX) {
                    // SOC local alto: proteger carga
                    digitalWrite(TRANSISTOR_PIN, LOW);  // Encender transistor
                    digitalWrite(RELAY_PIN, HIGH);      // Desconectar relé
                    bms_state = BMS_SOBRECARGA;
                    Serial.println("[BMS] Estado: SOBRECARGA");
                } else if (fabs(i_converter_local) > I_MAX) {
                    // Corriente excesiva → desconexión definitiva
                    digitalWrite(RELAY_PIN, HIGH);
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                    bms_state = BMS_SOBRECORRIENTE;
                    setLEDColor(0, 255, 0); // Rojo para sobrecorriente
                    Serial.println("[BMS] Estado: SOBRECORRIENTE");
                } else {
                    // Estado normal
                    digitalWrite(RELAY_PIN, LOW);
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                } break;

            case BMS_SOBREDESCARGA:
                if (soc_prom > SOC_MIN + 200) {
                    digitalWrite(RELAY_PIN, LOW);
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                    bms_state = BMS_NORMAL;
                    Serial.println("[BMS] Estado: NORMAL desde SOBREDESCARGA");
                } break;

            case BMS_SOBRECARGA:
                if (soc_local < SOC_MAX - 200) {
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                    digitalWrite(RELAY_PIN, LOW);
                    bms_state = BMS_NORMAL;
                    Serial.println("[BMS] Estado: NORMAL desde SOBRECARGA");
                } break;

            case BMS_SOBRECORRIENTE:
                // Apaga el sistema definitivamente
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(TRANSISTOR_PIN, HIGH);
                setLEDColor(0, 255, 0); // Rojo para sobrecorriente

                // Detiene la tarea de control si está activa
                if (handlePWM != NULL) {
                    vTaskSuspend(handlePWM);
                    Serial.println("[BMS] Control detenido por sobrecorriente");
                } break;
            }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}