#include "bms.h"

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

    // Inicializar pines de balanceo
    pinMode(TRANSISTOR_BALANCEO_1, OUTPUT);
    pinMode(TRANSISTOR_BALANCEO_2, OUTPUT);

    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
}

void BMS_Fan_Task(void *pvParameters) {
    while (true) {
        int16_t i_converter_local = 0;
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            i_converter_local = agentes_data[local_agent_id].i_converter;
            xSemaphoreGive(dataMutex);
        }

        float i_A = fabs(i_converter_local * 0.0001875 * 10.0);
        int duty = (int)(constrain(i_A, 0, 10.0) * 255.0 / 10.0);

        ledc_set_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL, duty);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, FAN_PWM_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void stopPWMOutput() {
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
    if (handlePWM != NULL) {
        vTaskSuspend(handlePWM);
    }
}

void BMS_State_Task(void *pvParameters) {
    while (true) {
        int16_t soc_local = 0;
        int16_t soc_prom = 0;
        int16_t i_converter_local = 0;
        int16_t v1_raw = 0, v2_raw = 0;
        float v1_volts, v2_volts;
        int16_t soc1, soc2, delta_soc;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            soc_local = agentes_data[local_agent_id].soc;
            i_converter_local = agentes_data[local_agent_id].i_converter;
            v1_raw = agentes_data[local_agent_id].v_cell1;
            v2_raw = agentes_data[local_agent_id].v_cell2;

            v1_volts = ads1.computeVolts(v1_raw);
            v2_volts = ads1.computeVolts(v2_raw);
            soc1 = soc_from_voltage(v1_volts);
            soc2 = soc_from_voltage(v2_volts);
            delta_soc = abs(soc1 - soc2);

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
                    stopPWMOutput();
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    bms_state = BMS_SOBREDESCARGA;
                    Serial.println("[BMS] Estado: SOBREDESCARGA");
                } else if (soc_local > SOC_MAX) {
                    stopPWMOutput();
                    digitalWrite(TRANSISTOR_BALANCEO_1, LOW);
                    digitalWrite(TRANSISTOR_BALANCEO_2, LOW);
                    bms_state = BMS_SOBRECARGA;
                    Serial.println("[BMS] Estado: SOBRECARGA");
                } else if (fabs(i_converter_local) > I_MAX) {
                    stopPWMOutput();
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    bms_state = BMS_SOBRECORRIENTE;
                    setLEDColor(0, 255, 0); // Verde
                    Serial.println("[BMS] Estado: SOBRECORRIENTE");
                } else if (delta_soc > DELTA_SOC_BALANCEO) {
                    if (soc1 > soc2) {
                        digitalWrite(TRANSISTOR_BALANCEO_1, LOW);
                        digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                        bms_state = BMS_BALANCEO_CELDA_1;
                        Serial.println("[BMS] Estado: BALANCEO CELDA 1");
                    } else {
                        digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                        digitalWrite(TRANSISTOR_BALANCEO_2, LOW);
                        bms_state = BMS_BALANCEO_CELDA_2;
                        Serial.println("[BMS] Estado: BALANCEO CELDA 2");
                    }
                } else {
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    if (handlePWM != NULL) vTaskResume(handlePWM);
                }
                break;

            case BMS_SOBREDESCARGA:
                if (soc_prom > SOC_MIN + 200) {
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    bms_state = BMS_NORMAL;
                    Serial.println("[BMS] Estado: NORMAL desde SOBREDESCARGA");
                }
                break;

            case BMS_SOBRECARGA:
                if (soc_local < SOC_MAX - 200) {
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    bms_state = BMS_NORMAL;
                    Serial.println("[BMS] Estado: NORMAL desde SOBRECARGA");
                }
                break;

            case BMS_SOBRECORRIENTE:
                // Ya suspendido, mantener duty en 0
                ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, 0);
                ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
                digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                break;

            case BMS_BALANCEO_CELDA_1:
            case BMS_BALANCEO_CELDA_2:
                if (delta_soc < DELTA_SOC_BALANCEO) {
                    digitalWrite(TRANSISTOR_BALANCEO_1, HIGH);
                    digitalWrite(TRANSISTOR_BALANCEO_2, HIGH);
                    bms_state = BMS_NORMAL;
                    Serial.println("[BMS] Estado: NORMAL desde BALANCEO");
                }
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
