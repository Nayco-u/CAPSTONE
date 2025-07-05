#include "bms.h"

static BMSState bms_state = BMS_NORMAL;

void initBMS() {
    // Configura el PWM para el ventilador
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

    // Inicializa pines de relé y transistor
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(TRANSISTOR_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);        // Relé normalmente cerrado (conectado)
    digitalWrite(TRANSISTOR_PIN, LOW);    // Transistor apagado
}

// Tarea para controlar el ventilador según la corriente de salida del conversor
void BMS_Fan_Task(void *pvParameters) {
    while (true) {
        int16_t i_converter_local = 0;
        // Lee la corriente de salida del conversor de manera segura
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            i_converter_local = agentes_data[local_agent_id].i_converter;
            xSemaphoreGive(dataMutex);
        }

        // Usa el valor absoluto de la corriente
        float i_A = fabs(i_converter_local * 0.0001875 * 10.0);
        int duty = (int)(constrain(i_A, 0, 10.0) * 255.0 / 10.0); // 0A=0%, 10A=100%

        ledc_set_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(200)); // Actualiza cada 200 ms
    }
}

// Tarea de máquina de estados del BMS
void BMS_State_Task(void *pvParameters) {
    while (true) {
        int16_t soc_local = 0;
        int16_t soc_prom = 0;

        // Lee los SOC de forma segura
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            soc_local = agentes_data[local_agent_id].soc;
            // Calcula el promedio de SOC (incluye local y remotos activos)
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

        // Lógica de la máquina de estados
        switch (bms_state) {
            case BMS_NORMAL:
                if (soc_prom < SOC_MIN) {
                    // SOC promedio bajo: desconecta el conversor (abre relé)
                    digitalWrite(RELAY_PIN, LOW);
                    bms_state = BMS_SOBREDESCARGA;
                } else if (soc_local > SOC_MAX) {
                    // SOC local alto: activa transistor y desconecta conversor
                    digitalWrite(TRANSISTOR_PIN, HIGH);
                    digitalWrite(RELAY_PIN, LOW);
                    bms_state = BMS_SOBRECARGA;
                } else {
                    // Estado normal: relé cerrado, transistor apagado
                    digitalWrite(RELAY_PIN, HIGH);
                    digitalWrite(TRANSISTOR_PIN, LOW);
                }
                break;
            case BMS_SOBREDESCARGA:
                if (soc_prom > SOC_MIN + 200) { // Histeresis para reconexión
                    digitalWrite(RELAY_PIN, HIGH);
                    bms_state = BMS_NORMAL;
                }
                break;
            case BMS_SOBRECARGA:
                if (soc_local < SOC_MAX - 200) { // Histeresis para apagar transistor
                    digitalWrite(TRANSISTOR_PIN, LOW);
                    digitalWrite(RELAY_PIN, HIGH);
                    bms_state = BMS_NORMAL;
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Actualiza cada 500 ms
    }
}